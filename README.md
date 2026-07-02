# libccsocket — Cross-Platform Socket Abstraction Library

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Language: C](https://img.shields.io/badge/Language-C99-blue.svg)
[![CI](https://github.com/CandyMi/ccsocket/actions/workflows/ci.yml/badge.svg)](https://github.com/CandyMi/ccsocket/actions/workflows/ci.yml)

A lightweight, portable C library that provides a unified API for network and inter-process communication across POSIX (Linux, macOS, FreeBSD, Solaris, AIX) and Windows. It wraps raw system sockets behind a consistent interface — handling platform quirks so you don't have to.

Included sub-modules:
- **ccicmp** — portable ICMP echo ("ping") library with IPv4/IPv6 support and timestamp-based RTT measurement.
- **ccdns** — DNS wire-format client (RFC 1035) with A/AAAA/CNAME/TXT/MX queries, EDNS (including Client Subnet, RFC 7871), and TCP mode.

---

## Features

- **TCP, UDP, ICMP, Unix Domain Sockets** — single API, multiple protocols
- **DNS client (A/AAAA/TXT/MX/CNAME)** — built-in resolver with TCP mode, EDNS (including Client Subnet), multi-NS retry
- **Cross-platform** — Linux, macOS, FreeBSD, Solaris, AIX, Windows
- **Zero-copy file transfer** — `sendfile()` on supported kernels, transparent fallback elsewhere
- **Scatter/gather I/O** — `iovec`-based send/recv with platform-safe accessor macros
- **Receive buffer query** — `ccsocket_get_nread()` returns available bytes via `FIONREAD` without consuming data
- **Exposed raw `bind()`** — `ccsocket_bind()` separates address binding from listening, useful for UDP, ICMP, and custom protocol setups
- **Non-blocking I/O** — integrated wait-state signaling via `CC_OPCODE_WAIT`
- **Load-balanced listeners** — `SO_REUSEPORT` / `SO_REUSEPORT_LB` for multi-process servers
- **IPv4 + IPv6 ICMP ping** — with RFC-compliant pseudo-header checksums
- **No external dependencies** — uses only OS-native socket APIs and standard C headers

---

## Quick Start

### Prerequisites

- **C compiler**: GCC, Clang, MSVC, or MinGW (C99)
- **CMake** ≥ 3.0

### Building

```bash
# Clone the repository
git clone <repo-url> libccsocket
cd libccsocket

# Configure (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the library (ccsocket + ccicmp in one target)
cmake --build build

# List available targets
cmake --build build --target help
```

### Integration

```cmake
# CMakeLists.txt of your project
find_package(ccsocket REQUIRED)
target_link_libraries(myapp PRIVATE ccsocket::ccsocket)
```

Both `ccsocket_*` and `ccicmp_*` APIs are available through the single `ccsocket` target.

### Minimal Example: TCP Echo Client

```c
#include "ccsocket.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    ccsocket_t s = ccsocket(CC_INET4, CC_TCP);
    if (s == INVALID_SOCKET) return 1;

    if (!ccsocket_connect(s, "127.0.0.1", 8080)) {
        ccsocket_close(s);
        return 1;
    }

    const char *msg = "Hello, libccsocket!";
    int sent;
    ccsocket_stcode_t r = ccsocket_send(s, msg, strlen(msg), &sent);
    if (r != CC_OPCODE_OK) {
        ccsocket_close(s);
        return 1;
    }

    char buf[256];
    int recved;
    r = ccsocket_recv(s, buf, sizeof(buf) - 1, &recved);
    if (r == CC_OPCODE_OK) {
        buf[recved] = '\0';
        printf("Received: %s\n", buf);
    }

    ccsocket_close(s);
    return 0;
}
```

### Minimal Example: TCP Echo Server

`ccsocket_listen()` handles both bind and listen in one call:

```c
#include "ccsocket.h"
#include <stdio.h>

int main(void)
{
    ccsocket_t s = ccsocket(CC_INET4, CC_TCP);
    if (s == INVALID_SOCKET) return 1;

    if (!ccsocket_listen(s, "127.0.0.1", 8080, -1)) {
        ccsocket_close(s);
        return 1;
    }

    ccsocket_t client = ccsocket_accept(s, CC_NOFLAG);
    if (client == INVALID_SOCKET) {
        ccsocket_close(s);
        return 1;
    }

    char buf[256];
    int recved;
    ccsocket_stcode_t r = ccsocket_recv(client, buf, sizeof(buf) - 1, &recved);
    if (r == CC_OPCODE_OK) {
        buf[recved] = '\0';
        ccsocket_send(client, buf, recved, NULL);
    }

    ccsocket_close(client);
    ccsocket_close(s);
    return 0;
}
```

### Minimal Example: UDP Listener (bind-only)

For UDP or ICMP, use `ccsocket_bind()` directly — no listen needed:

```c
#include "ccsocket.h"
#include <stdio.h>

int main(void)
{
    ccsocket_t s = ccsocket(CC_INET4, CC_UDP);
    if (s == INVALID_SOCKET) return 1;

    if (!ccsocket_bind(s, "127.0.0.1", 8080)) {
        ccsocket_close(s);
        return 1;
    }

    char buf[256];
    int recved;
    ccsocket_stcode_t r = ccsocket_recvfrom(s, buf, sizeof(buf) - 1, NULL, NULL, &recved);
    if (r == CC_OPCODE_OK) {
        buf[recved] = '\0';
        printf("Received: %s\n", buf);
    }

    ccsocket_close(s);
    return 0;
}
```

### Minimal Example: ICMP Ping

`ccicmp_init()` tries a privilege-free `SOCK_DGRAM` ICMP socket first
(Linux, macOS), then falls back to `SOCK_RAW` (requires root).

```c
#include "ccicmp.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    struct ccicmp_t ping;
    if (!ccicmp_init(&ping, CC_INET4)) {
        fprintf(stderr, "ccicmp_init failed\n");
        return 1;
    }

    if (ccicmp_echo(&ping, "1.1.1.1", "hello", 5)) {
        char reply[64];
        size_t len = sizeof(reply);
        if (ccicmp_reply(&ping, reply, &len)) {
            printf("Got reply, payload len = %zu\n", len);
        }
    }

    ccicmp_close(&ping);
    return 0;
}
```

---

## Build, Test & Run

```bash
# Debug build with tests
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure -V

# Run a single test
ctest --test-dir build -R ccsocket/tcp -V
```

### Test Suite (17 tests)

| Test | Verifies |
|---|---|
| `ccsocket/smoke` | Types, enums, iovec macros |
| `ccicmp/smoke` | ccicmp_t layout, symbols |
| `ccsocket/tcp` | **TCP loopback**: listen/accept/connect/send/recv |
| `ccsocket/udp` | **UDP**: connect, get_family, non-blocking WAIT |
| `ccsocket/pair` | **socketpair**: bidirectional send/recv |
| `ccsocket/addr` | **Address**: get_version, getaddrinfo |
| `ccsocket/opts` | **Options**: nodelay, keepalive, nonblock, cloexec |
| `ccsocket/msg` | **recvmsg/sendmsg**: CMSG macros, sendto/recvfrom, TCP socketpair round-trip |
| `ccsocket/http` | **HTTP text protocol**: request/response with `httpc.txt` |
| `ccicmp/ping` | **ICMP (IPv4 + IPv6)**: RFC 1071 / RFC 4443 checksum, packet layout, lifecycle |
| `ccdns/test` | **DNS**: query encode, response decode (A/AAAA/CNAME/TXT/MX), compression ptr, TCP mode, ECS (RFC 7871) |
| `ccsocket/family` | **Family/protocol**: enum boundary values, get_family, get_protocol |
| `ccsocket/ipv6` | **IPv6 loopback**: TCP + UDP over "::1" |
| `ccsocket/error` | **Error paths**: INVALID_SOCKET, NULL params, edge cases across all APIs |
| `ccsocket/sendfile` | **sendfile**: zero-copy file transfer via socketpair, data integrity |
| `ccsocket/connect-state` | **Connection state**: is_connected — TCP connected→CONNECTED (strict), non-blocking connecting, INVALID_SOCKET, UDP |
| `ccsocket/cxx-embed` | **C++ embedding**: headers compile under C++, `extern "C"` symbols linkable |

> ICMP lifecycle test works without root — init failure is handled gracefully. Full echo/reply needs `CAP_NET_RAW` / root.

---

## Project Structure

```
.
├── CMakeLists.txt      # Build system — CMake 3.0+, C99
├── include/            # Public API headers
│   ├── ccsocket.h      # Cross-platform socket API
│   ├── ccicmp.h        # ICMP echo (ping) API
│   └── ccdns.h         # DNS client API
├── src/                # Implementation sources
│   ├── ccsocket.c      # Core socket implementation (~1747 lines)
│   ├── ccicmp.c        # ICMP echo/response logic (~428 lines)
│   └── ccdns.c         # DNS encode/decode (~374 lines)
├── httpc.txt           # Sample HTTP/1.1 request (test fixture)
├── LICENSE             # MIT license
├── .gitignore          # Build artifacts
├── Doxyfile.in         # Doxygen config template
├── tests/              # Test suite (CTest, 17 tests)
│   ├── CMakeLists.txt
│   ├── test_ccsocket_smoke.c
│   ├── test_ccsocket_tcp.c
│   ├── test_ccsocket_udp.c
│   ├── test_ccsocket_pair.c
│   ├── test_ccsocket_addr.c
│   ├── test_ccsocket_opts.c
│   ├── test_ccsocket_msg.c
│   ├── test_ccsocket_http.c
│   ├── test_ccicmp_smoke.c
│   ├── test_ccicmp_ping.c
│   ├── test_ccdns.c
│   ├── test_ccsocket_family.c
│   ├── test_ccsocket_ipv6.c
│   ├── test_ccsocket_error.c
│   ├── test_ccsocket_sendfile.c
│   ├── test_ccsocket_connect_state.c
│   └── test_ccsocket_cxx_embed.cpp
├── .github/            # GitHub Actions CI workflows
│   └── workflows/
│       └── ci.yml
├── cmake/              # CMake package config templates
│   └── ccsocketConfig.cmake.in
├── AGENTS.md           # AI coding agent instructions
└── README.md           # ← this file
```

> `ccicmp` and `ccdns` are compiled as part of the `ccsocket` target — a single library provides both APIs. Use `ccsocket::ccsocket` to link.

---

## Platform Support

| Feature | Linux | macOS | FreeBSD | Windows | Solaris | AIX |
|---|---|---|---|---|---|---|
| TCP / UDP | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| ICMP | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Unix Domain Sockets | ✅ | ✅ | ✅ | ✅¹ | ✅ | ✅ |
| `socketpair` | ✅ | ✅ | ✅ | ✅² | ✅ | ✅ |
| `sendfile` (zero-copy) | ✅ | ✅ | ✅ | ❌³ | ✅ | ✅ |
| Load-balanced listen | ✅⁴ | ✅⁴ | ✅⁵ | ❌ | ✅⁴ | ✅⁴ |

¹ Windows 10 RS2+ (1703) and later.  
² Emulated via TCP loopback.  
³ Falls back to `read()` + `send()`.  
⁴ `SO_REUSEPORT`.  
⁵ `SO_REUSEPORT_LB` (FreeBSD 12+).

---

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE) for the full text.

Copyright © 2025 [CandyMi](https://github.com/CandyMi).

---

> AI coding agents: see [AGENTS.md](AGENTS.md) for project conventions.
