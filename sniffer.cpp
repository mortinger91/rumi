#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/bpf.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include "port_finder.h"
#include <iostream>

#ifndef _PCAP_BPF
#define _PCAP_BPF

typedef struct {
    char deviceName[11];
    char interfaceName[16];
    unsigned int bufferLength;
} BpfOption;

typedef struct {
    int fd;
    char deviceName[11];
    unsigned int bufferLength;
    unsigned int lastReadLength;
    unsigned int readBytesConsumed;
    char *buffer;
} BpfSniffer;

typedef struct {
    char *data;
} CapturedInfo;

void print_bpf_options(BpfOption option);

void print_bpf_sniffer_params(BpfSniffer sniffer);

int new_bpf_sniffer(BpfOption option, BpfSniffer *sniffer);

int read_bpf_packet_data(BpfSniffer *sniffer, CapturedInfo *info);

int close_bpf_sniffer(BpfSniffer *sniffer);

#endif /* _PCAP_BPF */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/bpf.h>
#include <netinet/in.h>
#include <net/if.h>

//#include "bpf.h"

void print_bpf_options(BpfOption option)
{
    fprintf(stderr, "BpfOption:\n");
    fprintf(stderr, "  BPF Device: %s\n", option.deviceName);
    fprintf(stderr, "  Network Interface: %s\n", option.interfaceName);
    fprintf(stderr, "  Buffer Length: %d\n", option.bufferLength);
}

void print_bpf_sniffer_params(BpfSniffer sniffer)
{
    fprintf(stderr, "BpfSniffer:\n");
    fprintf(stderr, "  Opened BPF Device: %s\n", sniffer.deviceName);
    fprintf(stderr, "  Buffer Length: %d\n", sniffer.bufferLength);
}

int pick_bpf_device(BpfSniffer *sniffer)
{
    char dev[11] = {0};
    for (int i = 0; i < 99; ++i) {
        sprintf(dev, "/dev/bpf%i", i);
        sniffer->fd = open(dev, O_RDWR);
        if (sniffer->fd != -1) {
            strcpy(sniffer->deviceName, dev);
            return 0;
        }
    }
    return -1;
}

int new_bpf_sniffer(BpfOption option, BpfSniffer *sniffer)
{
    if (strlen(option.deviceName) == 0) {
        if (pick_bpf_device(sniffer) == -1)
            return -1;
    } else {
        sniffer->fd = open(option.deviceName, O_RDWR);
        if (sniffer->fd != -1)
            return -1;
    }

    if (option.bufferLength == 0) {
        /* Get Buffer Length */
        if (ioctl(sniffer->fd, BIOCGBLEN, &sniffer->bufferLength) == -1) {
            perror("ioctl BIOCGBLEN");
            return -1;
        }
    } else {
        /* Set Buffer Length */
        /* The buffer must be set before the file is attached to an interface with BIOCSETIF. */
        if (ioctl(sniffer->fd, BIOCSBLEN, &option.bufferLength) == -1) {
            perror("ioctl BIOCSBLEN");
            return -1;
        }
        sniffer->bufferLength = option.bufferLength;
    }

    struct ifreq interface;
    strcpy(interface.ifr_name, option.interfaceName);
    if(ioctl(sniffer->fd, BIOCSETIF, &interface) > 0) {
        perror("ioctl BIOCSETIF");
        return -1;
    }

    unsigned int enable = 1;
    if (ioctl(sniffer->fd, BIOCIMMEDIATE, &enable) == -1) {
        perror("ioctl BIOCIMMEDIATE");
        return -1;
    }

    if (ioctl(sniffer->fd, BIOCPROMISC, NULL) == -1) {
        perror("ioctl BIOCPROMISC");
        return -1;
    }

    sniffer->readBytesConsumed = 0;
    sniffer->lastReadLength = 0;
    sniffer->buffer = (char*)malloc(sizeof(char) * sniffer->bufferLength);
    return 0;
}

int read_bpf_packet_data(BpfSniffer *sniffer, CapturedInfo *info)
{
    struct bpf_hdr *bpfPacket;
    if (sniffer->readBytesConsumed + sizeof(sniffer->buffer) >= sniffer->lastReadLength) {
        sniffer->readBytesConsumed = 0;
        memset(sniffer->buffer, 0, sniffer->bufferLength);

        ssize_t lastReadLength = read(sniffer->fd, sniffer->buffer, sniffer->bufferLength);
        if (lastReadLength == -1) {
            sniffer->lastReadLength = 0;
            perror("read bpf packet:");
            return -1;
        }
        sniffer->lastReadLength = (unsigned int) lastReadLength;
    }

    bpfPacket = (struct bpf_hdr*)((long)sniffer->buffer + (long)sniffer->readBytesConsumed);
    info->data = sniffer->buffer + (long)sniffer->readBytesConsumed + bpfPacket->bh_hdrlen;
    sniffer->readBytesConsumed += BPF_WORDALIGN(bpfPacket->bh_hdrlen + bpfPacket->bh_caplen);
    return bpfPacket->bh_datalen;
}

int close_bpf_sniffer(BpfSniffer *sniffer)
{
    free(sniffer->buffer);

    if (close(sniffer->fd) == -1)
        return -1;
    return 0;
}

int main(int argc, char** argv)
{
    using std::cout;

    BpfOption option;
    strcpy(option.interfaceName, "en0");
    option.bufferLength = 32767;
    print_bpf_options(option);

    BpfSniffer sniffer;
    if (new_bpf_sniffer(option, &sniffer) == -1)
        return 1;

    print_bpf_sniffer_params(sniffer);

    CapturedInfo info;
    int dataLength;
    while((dataLength = read_bpf_packet_data(&sniffer, &info)) != -1) {
        struct ether_header* eh = (struct ether_header*)info.data;

        if(ntohs(eh->ether_type) == ETHERTYPE_IPV6)
        {
            char src_addr[INET6_ADDRSTRLEN];
            char dst_addr[INET6_ADDRSTRLEN];
            struct ip6_hdr* ip = (struct ip6_hdr*)((long)eh + sizeof(struct ether_header));
            struct tcphdr* tcp = (struct tcphdr*)((long)ip + sizeof(struct ip6_hdr)); //(ip-> ip6_plen));
            inet_ntop(AF_INET6, &ip->ip6_src, src_addr, INET6_ADDRSTRLEN);
            inet_ntop(AF_INET6, &ip->ip6_dst, dst_addr, INET6_ADDRSTRLEN);
            if(ip->ip6_nxt == IPPROTO_TCP)
            {
                const std::string fileName{argv[1]};
                const auto ports{PortFinder::ports({fileName}, IPv6)};

                const auto sourcePort{ntohs(tcp->th_sport)};
                const auto destPort{ntohs(tcp->th_dport)};

                //for(auto port : ports) cout << "port is: " << port;

                if(ports.contains(sourcePort))
                {
                    cout << "  src ip: " << src_addr << std::endl;
                    printf("  dst ip: %s\n", dst_addr);
                    printf("  dst port: %d\n", destPort);
                    printf("  src port: %d\n", sourcePort);
                    fflush(stdout);
                }
            }
        }
        else if(ntohs(eh->ether_type) == ETHERTYPE_IP)
        {
            struct ip* ip = (struct ip*)((long)eh + sizeof(struct ether_header));
            struct tcphdr* tcp = (struct tcphdr*)((long)ip + (ip->ip_hl * 4));

            if (ip->ip_p == IPPROTO_TCP) {
                const std::string fileName{argv[1]};
                const auto ports{PortFinder::ports({fileName}, IPv4)};
                const auto sourcePort{ntohs(tcp->th_sport)};
                const auto destPort{ntohs(tcp->th_dport)};

//                for(auto port : ports) cout << "port is: " << port;

                if(ports.contains(sourcePort))
                {
                    printf("  src ip: %s\n", inet_ntoa(ip->ip_src));
                    printf("  dst ip: %s\n", inet_ntoa(ip->ip_dst));
                    printf("  dst port: %d\n", destPort);
                    printf("  src port: %d\n", sourcePort);
                    fflush(stdout);
                }
            }
        } else {
            //printf("  type: Other, %x\n", eh->ether_type);
        }
    }
    close_bpf_sniffer(&sniffer);
    return 0;
}