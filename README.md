# libccsocket — Cross-Platform Socket Abstraction Library

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Language: C](https://img.shields.io/badge/Language-C11-blue.svg)
[![Build](https://img.shields.io/badge/Build-CMake_3.10+-brightgreen.svg)]()
[![Tests](https://img.shields.io/badge/Tests-9/9-passing-brightgreen.svg)]()

A lightweight, portable C library that provides a unified API for network and inter-process communication across POSIX (Linux, macOS, FreeBSD, Solaris, AIX) and Windows. It wraps raw system sockets behind a consistent interface — handling platform quirks so you don't have to.

Included sub-module: **ccicmp** — a portable ICMP echo ("ping") library with IPv4/IPv6 support and timestamp-based RTT measurement.

---

## Features

- **TCP, UDP, ICMP, Unix Domain Sockets** — single API, multiple protocols
- **Cross-platform** — Linux, macOS, FreeBSD, Solaris, AIX, Windows
- **Zero-copy file transfer** — `sendfile()` on supported kernels, transparent fallback elsewhere
- **Scatter/gather I/O** — `iovec`-based send/recv with platform-safe accessor macros
- **Non-blocking I/O** — integrated wait-state signaling via `CC_OPCODE_WAIT`
- **Load-balanced listeners** — `SO_REUSEPORT` / `SO_REUSEPORT_LB` for multi-process servers
- **IPv4 + IPv6 ICMP ping** — with RFC-compliant pseudo-header checksums
- **No external dependencies** — uses only OS-native socket APIs and standard C headers

---

## Quick Start

### Prerequisites

- **C compiler**: GCC, Clang, MSVC, or MinGW (C11 recommended, C99 minimum)
- **CMake** ≥ 3.10

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

### Minimal Example: ICMP Ping

```c
#include "ccicmp.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    struct ccicmp_t ping;
    if (!ccicmp_init(&ping, CC_INET4)) {
        fprintf(stderr, "ccicmp_init failed (need root/CAP_NET_RAW)\n");
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

# Run all 9 tests
ctest --test-dir build --output-on-failure -V

# Run a single test
ctest --test-dir build -R ccsocket/tcp -V
```

### Test Suite (9 tests)

| Test | Verifies |
|---|---|
| `ccsocket/smoke` | Types, enums, iovec macros |
| `ccicmp/smoke` | ccicmp_t layout, symbols |
| `ccsocket/tcp` | **TCP loopback**: listen/accept/connect/send/recv |
| `ccsocket/udp` | **UDP**: connect, get_family, non-blocking WAIT |
| `ccsocket/pair` | **socketpair**: bidirectional send/recv |
| `ccsocket/addr` | **Address**: get_version, getaddrinfo |
| `ccsocket/opts` | **Options**: nodelay, keepalive, nonblock, cloexec |
| `ccsocket/http` | **HTTP text protocol**: request/response with `httpc.txt` |
| `ccicmp/ping` | **ICMP**: RFC 1071 checksum, packet layout, lifecycle |

> ICMP lifecycle test works without root — init failure is handled gracefully. Full echo/reply needs `CAP_NET_RAW` / root.

---

## Project Structure

```
.
├── CMakeLists.txt      # Build system — CMake 3.10+, C11
├── ccsocket.h          # Public API — types, enums, function declarations
├── ccsocket.c          # Core socket implementation (~1058 lines)
├── ccicmp.h            # ICMP ping public API
├── ccicmp.c            # ICMP echo/response implementation (~306 lines)
├── httpc.txt           # Sample HTTP/1.1 request (test fixture)
├── LICENSE             # MIT license
├── .gitignore          # Build artifacts
├── tests/              # Test suite (CTest, 9 tests)
│   ├── CMakeLists.txt
│   ├── test_ccsocket_smoke.c
│   ├── test_ccsocket_tcp.c
│   ├── test_ccsocket_udp.c
│   ├── test_ccsocket_pair.c
│   ├── test_ccsocket_addr.c
│   ├── test_ccsocket_opts.c
│   ├── test_ccsocket_http.c
│   ├── test_ccicmp_smoke.c
│   └── test_ccicmp_ping.c
├── AGENTS.md           # AI coding agent instructions
└── README.md           # ← this file
```

> `ccicmp` is compiled as part of the `ccsocket` target — a single library provides both APIs. Use `ccsocket::ccsocket` to link.

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
