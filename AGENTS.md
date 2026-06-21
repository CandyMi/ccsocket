# AGENTS.md

> Project-level constraints and conventions for AI coding agents working on the `libccsocket` / `ccicmp` codebase.
> This file follows the [AGENTS.md specification](https://agents.md/).

---

## 1. Project Overview

### 1.1 Identity

| Field | Value |
|---|---|
| **Project** | `libccsocket` — Cross-platform Socket Abstraction Library |
| **Author** | [CandyMi](https://github.com/CandyMi) |
| **License** | MIT — see [LICENSE](LICENSE) |
| **Language** | C (C99 build standard) |
| **Status** | Active development |
| **Repository** | *(private / self-hosted)* |

### 1.2 Purpose

`libccsocket` provides a unified, portable C API for network and inter-process communication across POSIX (Linux, macOS, FreeBSD, Solaris, AIX) and Windows. It wraps raw system sockets (`socket`, `connect`, `send`, `recv`, `accept`, etc.) behind a consistent interface, handling platform quirks (socketpair emulation on Windows, IPv6 ICMP pseudo-header checksums, `sendfile` fallback paths) transparently.

`ccicmp` is a thin ICMP echo ("ping") library built on top of `libccsocket`. It supports both IPv4 and IPv6, timestamp-based RTT measurement, and configurable payloads.

---

## 2. Repository Structure

```
.
├── CMakeLists.txt      # Root build system — CMake 3.0+, C99 standard
├── ccsocket.h          # Public API header — types, enums, macros, exported function declarations
├── ccsocket.c          # Implementation — ~1058 lines, all platform backends in one translation unit
├── ccicmp.h            # Public API header — ICMP context struct, function declarations
├── ccicmp.c            # Implementation — ~374 lines, ICMP echo/response logic
├── ccdns.h             # Public API header — DNS client context, function declarations
├── ccdns.c             # Implementation — ~374 lines, DNS wire-format encode/decode (RFC 1035), TCP mode (RFC 1035 §4.2.2), TXT (RFC 1035 §3.3), MX (RFC 1035 §3.3.9)
├── httpc.txt           # Sample HTTP/1.1 request payload (test fixture)
├── LICENSE             # MIT license text
├── .gitignore          # Build artifacts, IDE configs, object files
├── .vscode/            # Editor workspace settings (not part of the library)
│   └── settings.json
├── .github/            # GitHub Actions CI workflows
│   └── workflows/
│       └── ci.yml
├── tests/              # Test suite — 11 tests via CTest
│   ├── CMakeLists.txt
│   ├── test_ccsocket_smoke.c
│   ├── test_ccsocket_tcp.c
│   ├── test_ccsocket_udp.c
│   ├── test_ccsocket_pair.c
│   ├── test_ccsocket_addr.c
│   ├── test_ccsocket_opts.c
│   ├── test_ccsocket_http.c
│   ├── test_ccicmp_smoke.c
│   ├── test_ccicmp_ping.c
│   ├── test_ccdns.c
│   └── test_ccsocket_cxx_embed.cpp
├── AGENTS.md           # ← this file
└── README.md           # Project introduction (English)
```

### 2.1 Module Dependency

```
┌─────────────────────────────────────┐
│           libccsocket               │
│  ┌────────────┐  ┌──────────────┐   │
│  ┌────────────┐  ┌──────────────┐   │
│  │  ccdns     │  │  ccicmp      │   │
│  │ (DNS clnt) │  │ (ICMP ping)  │   │
│  └─────┬──────┘  └──────┬───────┘   │
│        │                │           │
│        └────┬───┬───────┘           │
│             ▼   ▼                   │
│      ┌──────────────────┐           │
│      │  ccsocket        │           │
│      │  (socket ABI)    │           │
│      └────────┬─────────┘           │
│               │                     │
│     ┌─────────┴──────────┐          │
│     ▼                    ▼          │
│  ccicmp / ccdns    single library   │
│  built into same    target: ccsocket│
│  library target                     │
│  consuming ccsocket                 │
│  provides all APIs                  │
│  ┌──────────────────┐               │
│  │  ccdns uses      │               │
│  │  ccdns.h (.h)    │               │
│  │  no ccsocket.h   │               │
│  │  dependency      │               │
│  └──────────────────┘               │
└─────────────────────────────────────┘
                  │
     ┌────────────┴────────────┐
     ▼                         ▼
POSIX Sockets            WinSock2 (Windows)
(socket, sendmsg, ...)   (WSASocket, WSASend, ...)
```

> **Note**: `ccicmp` is compiled as part of the `ccsocket` library target. There is no separate `ccicmp` library — consuming `ccsocket` provides both APIs.

---

## 3. Language & Coding Conventions

### 3.1 Language Dialect

- **Build standard**: C99 (`CMAKE_C_STANDARD 99`)
- **Extensions**: C99 `inline` functions and `//` line comments are used freely. C89/C90 compatibility is preserved where practical but not enforced.
- **C++ Interop**: All public headers are wrapped in `extern "C"` for C++ consumers

### 3.2 Naming Conventions

| Category | Pattern | Example |
|---|---|---|
| Public API functions | `module_action[_qualifier]` | `ccsocket_connect`, `ccicmp_echo` |
| Internal (non-ABI) functions | `_module_action` or `module_action_internal` | `_ccsocket_set_flags`, `ccsocket_listen_internal` |
| Types / structs | `module_name_t` | `ccsocket_t`, `ccicmp_t`, `ccaddrinfo_t` |
| Enums | `CC_DESCRIPTIVE_NAME` | `CC_NONBLOCK`, `CC_OPCODE_WAIT` |
| Enum type names | `module_name_role_t` | `ccsocket_flags_t`, `ccsocket_stcode_t` |
| Macros (public) | `CC_UPPER_SNAKE` | `CC_INET4`, `CC_CLOEXEC` |
| Macros (internal) | `lower_snake` | `cc_alloca`, `ccsocket_dump` |

### 3.3 Comment Style

| Context | Style | Required? |
|---|---|---|
| Public API (headers) | `/** brief description \n * detail \n */` Doxygen | **Recommended** |
| Internal function doc | `/* brief */` or `/** brief */` | Preferred |
| Implementation notes | `/* explanation */` | Preferred |
| Inline annotations | `// short note` (C99+) or `/* note */` (C89) | Acceptable |
| TODO / FIXME | `/* TODO: reason */` or `// TODO: reason` | Use consistently |
| Platform #ifdef branches | `/* PlatformName: why */` | **Required** |

**Rules**:
- Chinese is acceptable in `.md` documentation files if needed.
- Source comments **must be in English**.
- Every public function declaration in `.h` files should have a Doxygen-style comment describing parameters, return value, and notable behaviors.

### 3.4 Error Handling

- Return status via `bool` (success/failure) or `ccsocket_stcode_t` / `ccsocket_conn_state_t` enums for richer status.
- Side-channel error details via `errno` (POSIX) or `WSAGetLastError()` (Windows).
- Use `ccsocket_init_errno()` / `ccsocket_is_errno()` / `ccsocket_set_errno()` macros for platform-portable errno access.
- Functions that can block should document their non-blocking behavior (e.g., "returns `CC_OPCODE_WAIT` when buffer is full").

### 3.5 Memory & Resource Management

- **Stack allocation preferred**: `alloca()` / `_alloca()` for temporary buffers (via `cc_alloca` macro).
- **No dynamic allocation in hot paths**: `malloc`/`free` used only in `ccsocket_getaddrinfo` / `ccsocket_freeaddrinfo`.
- **Socket lifetime**: `ccsocket_close()` must be called for every successfully created socket. No implicit garbage collection.
- **Ownership**: Functions named `_create` / `_init` return owned resources the caller must `_close` / `_destroy`.

### 3.6 Portability Patterns

- **Platform detection**: `#if _WIN32` for Windows vs. `#else` for POSIX.
- **Socket type**: `SOCKET` (Windows) vs `int` (POSIX), unified via `ccsocket_t`.
- **IO vectors**: Different field order on Windows (`len` first) vs POSIX (`buf` first) — always use accessor macros (`ccsocket_set_iov_len`, etc.), never direct struct access.
- **Inline**: `CC_INLINE` macro resolves to `static inline` (C99), `static __inline` (MSVC), or `static` (C89 fallback).

### 3.7 Git Commit Conventions

All commits **must** follow the [Conventional Commits](https://www.conventionalcommits.org/) format using **English only**:

```
type(scope): short description (≤ 72 chars)

Optional body — wrap at 72 chars.
Use bullet points for multi-line details.
```

**Allowed types:**

| Type | Usage |
|---|---|
| `feat` | New feature or public API addition |
| `fix` | Bug fix |
| `refactor` | Code restructuring with no behavior change |
| `docs` | Documentation only (AGENTS.md, README.md, Doxygen) |
| `build` | CMake, build system, CI |
| `test` | Test additions or fixes |
| `perf` | Performance improvement |
| `chore` | Maintenance, minor cleanup |

**Scope** refers to the affected module: `ccsocket`, `ccicmp`, `CMakeLists.txt`, `tests`, etc.

**Examples:**

```
feat(ccicmp): add CC_ICMP1 (SOCK_DGRAM+ICMP) privilege-free ping support

ccicmp_init() now tries SOCK_DGRAM first, falling back to SOCK_RAW.

fix(build): FreeBSD compilation (alloca.h / struct ip / SOCK_DGRAM)
test(ccicmp): add ICMPv6 pseudo-header checksum vectors
docs(AGENTS): add git commit conventions section
```

Multiple commits addressing the same feature should be squashed before pushing.

---

## 4. Build System

Build defined in [`CMakeLists.txt`](CMakeLists.txt) — CMake ≥ 3.0, C99.

### 4.1 Toolchain Requirements

- **C compiler**: GCC, Clang, MSVC, or MinGW — C99 mode or later
- **CMake**: ≥ 3.0
- **Doxygen** (optional): required only when `BUILD_DOCS=ON`
- **No external dependencies**: The library uses only OS-native socket APIs and standard C headers

### 4.2 Build Commands

```bash
# Configure (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build all targets
cmake --build build

# Build and run tests
cmake --build build && ctest --test-dir build --output-on-failure -V

# Configure (Debug, for development)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure -V

# Generate Doxygen API documentation (requires Doxygen)
cmake -B build -DBUILD_DOCS=ON
cmake --build build --target docs
# Open build/docs/html/index.html
```

### 4.3 CMake Options

| Option | Default | Affects | Purpose |
|---|---|---|---|
| `BUILD_SHARED_LIBS` | `ON` | all | Build as shared (.so/.dylib/.dll) vs static (.a/.lib) |
| `BUILD_TESTING` | `ON` | tests | Enable test targets |
| `CCICMP_MAX_PAYLOAD` | `65500` | ccicmp | Max ICMP payload size |
| `CCICMP_RECV_BUFSZ` | `65535` | ccicmp | ICMP receive buffer size |
| `BUILD_DOCS` | `OFF` | docs | Build Doxygen API documentation (requires Doxygen) |

Pass via `-D<OPTION>=<VALUE>` on the cmake command line.

When `BUILD_SHARED_LIBS=ON` on Windows, the build will also define `CCSOCKET_BUILD_SHARED` (library build) or `CCSOCKET_SHARED` (consumers) to select the correct `__declspec(dllexport)` / `__declspec(dllimport)` decoration automatically.

### 4.4 Library Targets

| CMake Target | Sources | Link Dependencies | Alias |
|---|---|---|---|
| `ccsocket` | `ccsocket.h/.c`, `ccicmp.h/.c`, `ccdns.h/.c` | `ws2_32` (Windows) | `ccsocket::ccsocket` |

### 4.5 Compile-Time Configuration Macros

| Macro | Default | Scope | Purpose |
|---|---|---|---|
| `CCICMP_MAX_PAYLOAD` | `65500` | ccicmp | Max ICMP payload size |
| `CCICMP_RECV_BUFSZ` | `65535` | ccicmp | ICMP receive buffer size |
| `NDEBUG` | *(unset)* | ccsocket | Disables debug `fprintf` tracing |
| `CCSOCKET_BUILD_SHARED` | *(auto)* | ccsocket | Set during shared library build to select dllexport |
| `CCSOCKET_SHARED` | *(auto)* | ccsocket | Set for shared library consumers to select dllimport |

---

## 5. Testing

Test infrastructure is live via CTest. Test sources live in [`tests/`](tests/).

### 5.1 Current Tests (11 total)

| Test | Type | What It Verifies |
|---|---|---|
| `ccsocket/smoke` | Compile-and-link | Enums, macros, iovec accessors, `ccsocket_get_version` |
| `ccicmp/smoke` | Compile-and-link | `ccicmp_t` layout, symbol resolution, invalid-domain rejection |
| `ccsocket/tcp` | Functional | TCP loopback: listen → accept → connect → send/recv round-trip |
| `ccsocket/udp` | Functional | UDP creation, connect, get_family/get_peername, non-blocking WAIT |
| `ccsocket/pair` | Functional | socketpair bidirectional send/recv data integrity |
| `ccsocket/addr` | Functional | IP version detection (`get_version`), `getaddrinfo` for localhost |
| `ccsocket/opts` | Functional | nodelay/reuseaddr/keepalive/nonblock/cloexec (valid + invalid handle) |
| `ccsocket/http` | **Combined protocol** | HTTP request/response round-trip using `httpc.txt` as template |
| `ccicmp/ping` | **Combined (ICMP)** | IPv4/IPv6 checksum (RFC 1071 / RFC 4443), packet layout, init/close lifecycle |
| `ccdns/test` | **DNS client** | DNS query encode, response decode (A/AAAA/CNAME/TXT/MX), compression ptr (RFC 1035 §4.1.4), TCP mode (RFC 1035 §4.2.2), lifecycle |
| `ccsocket/cxx-embed` | **C++ embedding** | All headers compile under C++, `extern "C"` symbols linkable, iovec/ICMP/DNS lifecycle smoke |

### 5.2 Test Conventions

- **Location**: `tests/` directory at project root
- **Naming**: `test_<module>_<scenario>.c` (e.g., `test_ccsocket_tcp_connect.c`)
- **Framework**: Plain `assert()` + CTest — no external test framework dependency
- **Adding a new test**: (1) create `tests/test_<module>_<scenario>.c`, (2) add `add_executable` / `add_test` in `tests/CMakeLists.txt`, (3) rebuild and run `ctest`

### 5.3 Running Tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure -V
```

### 5.4 Remaining Gaps

| Gap | Reason |
|---|---|
| ICMP echo/reply round-trip | Requires `CAP_NET_RAW` / root |
| ICMPv6 echo/reply round-trip | Same privilege requirement as IPv4 |
| sendfile functional test | Needs a temporary file descriptor |

---

## 6. CI Pipeline

CI is configured in [`.github/workflows/ci.yml`](.github/workflows/ci.yml) — GitHub Actions.

### 6.1 Coverage Matrix (9 jobs)

| Job | OS | Arch | Compiler | Notes |
|---|---|---|---|---|
| `linux-gcc` | Ubuntu (Debian) | x86_64 | GCC | Primary Linux |
| `linux-clang` | Ubuntu (Debian) | x86_64 | Clang | Alternate compiler |
| `linux-32bit` | Ubuntu (Debian) | i686 | GCC `-m32` | 32-bit via multilib |
| `centos7` | CentOS 7 (Docker) | x86_64 | GCC 4.8.5 | Legacy platform |
| `macos-x64` | macOS (Intel) | x86_64 | AppleClang | Intel Mac |
| `macos-arm64` | macOS (Apple Silicon) | arm64 | AppleClang | Native ARM |
| `windows-msvc-x64` | Windows | x64 | MSVC | Primary Windows |
| `windows-msvc-x86` | Windows | x86 | MSVC `-A Win32` | 32-bit Windows |
| `windows-mingw64` | Windows (MinGW) | x64 | MinGW-w64 (gcc) | MinGW toolchain |

### 6.2 What Each Job Verifies

1. **C build** — shared + static library compiles with `-Werror` / `/WX`
2. **CTest** — all 11 tests pass via `ctest --output-on-failure`
3. **C++ compile+link** — library `.c` files compiled as C++ (`-x c++`), verifying `extern "C"` interop
4. **C++ header-only** — all public headers included from a pure C++ TU
5. **Static-link smoke** — static library links a standalone executable (Linux)
6. **Zero warnings** — `-Werror` equivalent on all compilers

### 6.3 Trigger

- On push to any branch
- On pull request targeting `main` or `master`

---

## 7. Agent Task Constraints

### 6.1 Mandatory Pre-Check

**Before any edit, the agent MUST read this `AGENTS.md` file and verify compliance with all applicable sections.** If the proposed change conflicts with any convention or architectural invariant documented here, the agent **MUST NOT proceed automatically**. Instead, it must:

1. Explicitly describe the conflict in detail (citing the relevant AGENTS.md section).
2. Present an interactive choice (`ask_choice`) asking the user to approve, reject, or redirect.
3. Proceed only after receiving explicit user approval.

### 6.2 Conflict Examples That Require a Choice

- Adding a C++-only feature to a header that breaks C89 compatibility.
- Introducing a new external dependency when none currently exists.
- Changing the naming convention (e.g., using `camelCase` instead of `module_name`).
- Adding heap allocation to a hot path that currently uses stack-only allocation.
- Removing or modifying a public API function without updating its Doxygen comment.

### 6.3 Documentation Coherence

- **Every public API change** (add, remove, or modify a `CC*_EXPORT` function or a public type/enum/macro) **must** be documented via Doxygen in the source header (`.h`).
- Changes that affect platform behavior (e.g., a new `#ifdef` branch for a new OS) must be described in the Doxygen comment of the affected function(s).
- Internal (non-exported) changes do not require header documentation changes, but should be described in the commit message.

### 6.4 Update Currency

**All targeted modifications must update this `AGENTS.md` file to reflect the current state.** Specifically:

| Type of Change | AGENTS.md Section to Update |
|---|---|
| New file added to repository | §2 Repository Structure |
| New build dependency or tool | §4 Build System |
| New language convention decided | §3 Language & Coding Conventions |
| New compile-time macro added | §4.5 Compile-Time Configuration Macros |
| Test suite added or reorganized | §5 Testing |
| New public module created | §2.1 Module Dependency |
| License change | §1.1 Identity + LICENSE |

When updating AGENTS.md, the agent should consider whether the change has **extensions** that warrant documentation beyond the immediate diff — for example, a new platform `#ifdef` may also affect error-handling patterns in §3.4, a new test file may imply test framework choices in §5.1.

---

## 8. Design Decisions & Technical Notes

### 8.1 `ccsocket_iovec_t` Field Order

The `buf` and `len` fields are **reversed** between Windows (`len` first) and POSIX (`buf` first). This is intentional — it matches the native `WSABUF` / `struct iovec` layout. **Never access fields directly**; always use the `ccsocket_set_iov_*` / `ccsocket_get_iov_*` macros.

### 8.2 IPv6 ICMP Checksum

Per [RFC 4443 §2.3](https://datatracker.ietf.org/doc/html/rfc4443#section-2.3), the ICMPv6 checksum covers a 40-byte pseudo-header: `src(16) + dst(16) + len(4) + zero(3) + next_hdr(1)`.

- `getsockname()` → source address; `getpeername()` → destination address.
- On macOS raw sockets, `getsockname()` returns `::` — fall back to destination as source (correct for loopback).

### 8.3 Platform Behaviors

- **macOS/BSD raw ICMPv6**: Receive buffer does **not** contain the IPv6 header — data starts at ICMPv6 header.
- **Linux raw ICMPv6**: Receive buffer **includes** the 40-byte IPv6 header.
- `ccicmp_skip_ip_header()` auto-detects both cases.
- **Windows ICMP**: Raw socket ICMP is restricted; `ccicmp_init()` will fail. Callers must handle this case.
- **sendfile**: macOS/FreeBSD/Linux/Solaris use kernel `sendfile()` (zero-copy). Windows and other platforms fall back to `read()` + `send()`.

### 8.4 DNS over TCP (RFC 1035 §4.2.2)

DNS over TCP uses a **2-byte length prefix** before the standard DNS wire-format message.  The prefix is the message body length in network byte order, not including the 2 bytes themselves.

The TCP mode is controlled via the `tcp` field in `ccdns_t` (set via `ccdns_set_tcp()`):

- **`ccdns_encode()`**: when `tcp == true`, writes the DNS message body at `buf[2..]` and fills `buf[0..1]` with the body length.  The return value is the total bytes written (body + 2).
- **`ccdns_decode()`**: when `tcp == true`, skips `buf[0..1]` before parsing the DNS header and records.

**Compression pointer handling**: DNS name compression (RFC 1035 §4.1.4) uses offsets relative to the start of the DNS message, not the TCP transport buffer.  The internal `dns_name_decode()` accepts a `base_off` parameter to compensate — `base_off` is 0 for UDP mode and 2 for TCP mode.

**EDNS interaction**: EDNS OPT records are technically unnecessary over TCP (no 512-byte limit), but remain supported.  The EDNS and TCP flags are independent — callers may enable both.

### 8.5 Internal DNS Resolver

`ccsocket_getaddrinfo()` uses a built-in DNS resolver built on `ccdns` + `ccsocket` UDP sockets, rather than calling the system `getaddrinfo()`.  This preserves TTL information and provides cross-platform consistency (Windows `getaddrinfo` does not read `/etc/resolv.conf`).

The resolver supports retry and failover:

- **Multi-NS**: reads up to 4 nameservers from `/etc/resolv.conf` (POSIX) or registry (Windows).
- **Retry**: 2 rounds across all nameservers (glibc-style sequential), total up to 8 attempts.
- **Fallback**: when no nameserver is configured, defaults to `8.8.8.8`.

### 8.6 ICMP DGRAM vs RAW Semantics

`ccicmp_init()` automatically selects the best available socket type:

| Socket | Platforms | Privilege | ICMP header |
|--------|-----------|-----------|-------------|
| `SOCK_DGRAM` + `IPPROTO_ICMP` (CC_ICMP1) | Linux ≥ 3.0, macOS, FreeBSD | No root needed | **Linux**: kernel adds ICMP header (user sends payload only). **macOS/BSD**: user sends full ICMP header + checksum (kernel adds only IP header) |
| `SOCK_RAW` + `IPPROTO_ICMP` (CC_ICMP) | All POSIX | Requires root / `CAP_NET_RAW` | User sends full IP + ICMP header + checksum |

The runtime helper `ccsocket_get_protocol()` returns the OS socket type (`SOCK_DGRAM` / `SOCK_RAW` / `SOCK_STREAM`), used by `ccicmp_is_dgram()` inside echo/reply to select the correct I/O path.

---

## 9. Quick Reference

### 9.1 Common Agent Workflows

| Goal | Steps |
|---|---|
| Add a new socket option | (1) Add enum/constant to `ccsocket.h` (Doxygen), (2) implement in `ccsocket.c` with `#if` guards, (3) export via `CCSOCKET_EXPORT` |
| Add a new DNS feature | (1) Add to `ccdns.h` (Doxygen), (2) implement in `ccdns.c`, (3) export via `CCDNS_EXPORT`, (4) update `AGENTS.md` §2 and §5 |
| Port to a new OS | (1) Add `#if`/`#elif`/`#else` blocks in `ccsocket.c`, (2) update CC_INLINE / socket types if needed, (3) test via compile, (4) update platform matrix in Doxygen comments and §8.3 here |
| Add a new ICMP feature | (1) Add to `ccicmp.h` (Doxygen), (2) implement in `ccicmp.c`, (3) add compile-time macro to §4.3 if configurable, (4) rebuild — ccicmp is compiled as part of ccsocket |

### 9.2 File Modification Permission Levels

| Action | Permission |
|---|---|
| Edit `*.c` / `*.h` implementation | Allowed — must follow §3 conventions |
| Edit existing test files | Allowed — must preserve CTest integration |
| Edit `AGENTS.md` | **Required** when making structural changes (see §7.4) |
| Create new `.c` / `.h` files | Allowed — must update §2 |
| Edit `CMakeLists.txt` | Allowed — must preserve the library target and test discovery |
| Modify `LICENSE` | **Forbidden** without explicit user request |
| Modify `.gitignore` | Allowed for new artifact patterns |
