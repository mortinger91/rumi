#pragma once

#include <stdexcept>
#include <optional>
#include <vector>
#include <string>
#include <iostream>
#include <memory>
#include <array>
#include <set>
#include <span>
#include <variant>
#include <compare>
#include <functional>
#include <unistd.h>
#include <stdio.h>
#include <sys/errno.h>
#include <string.h>
#include <arpa/inet.h>

#if defined(__linux__)
#define CMB_LINUX
#elif defined(__APPLE__)
#define CMB_MACOS
#define CMB_MAC
#else
#define CMB_WIN
#define CMB_WINDOWS
#endif

// Types
using PortSet = std::set<std::uint16_t>;
enum IPVersion { IPv4, IPv6 };
