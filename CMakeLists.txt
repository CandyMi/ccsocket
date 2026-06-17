# ============================================================================
# libccsocket — Cross-Platform Socket Abstraction Library
# https://agents.md/  |  MIT License  |  Author: CandyMi
# ============================================================================

cmake_minimum_required(VERSION 3.10)

# -- Project ------------------------------------------------------------------
project(libccsocket
  VERSION     1.0.0
  DESCRIPTION "Cross-platform C socket abstraction library (TCP/UDP/ICMP/Unix)"
  LANGUAGES   C
)

# -- C Standard ---------------------------------------------------------------
# Baseline C99 (inline, stdbool.h, stdint.h, // comments).
# Prefer C11 when available; not required — C99 toolchains are still valid.
set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_C_EXTENSIONS        OFF)

# -- Build Type Default -------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# -- Options ------------------------------------------------------------------
option(BUILD_SHARED_LIBS "Build shared libraries (.so/.dylib/.dll)" ON)
option(BUILD_TESTING     "Build test suite"                         ON)

# Numeric compile-time configuration (CACHE STRING, NOT option() — option treats 65500 as OFF)
set(CCICMP_MAX_PAYLOAD 65500 CACHE STRING "Max ICMP payload bytes")
set(CCICMP_RECV_BUFSZ  65535 CACHE STRING "ICMP receive buffer size")

# -- Compile Warnings ---------------------------------------------------------
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  add_compile_options(-Wall -Wextra -Wpedantic)
  add_compile_options(-Wshadow)
  add_compile_options(-Wstrict-prototypes -Wold-style-definition)
  add_compile_options(-Wmissing-prototypes -Wmissing-declarations)
  # Treat some portability issues as errors
  add_compile_options(-Werror=implicit-function-declaration)
  add_compile_options(-Werror=return-type)
  # The following flags are strict but the existing codebase has pre-existing
  # issues. Uncomment when those are resolved:
  # add_compile_options(-Wcast-align -Wconversion -Wsign-conversion)
elseif(MSVC)
  add_compile_options(/W4 /utf-8)
endif()

# -- Symbol Visibility (non-Windows) ------------------------------------------
# CCSOCKET_EXPORT is handled per-symbol in the header;
# hide everything else by default.
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# -- Library: ccsocket (includes ccicmp + ccdns sub-modules) -----------------
add_library(ccsocket
  ccsocket.h
  ccsocket.c
  ccicmp.h
  ccicmp.c
  ccdns.h
  ccdns.c
)

target_include_directories(ccsocket
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(ccsocket
  PRIVATE
    $<$<BOOL:${BUILD_SHARED_LIBS}>:CCSOCKET_BUILD_SHARED>
    CCICMP_MAX_PAYLOAD=${CCICMP_MAX_PAYLOAD}
    CCICMP_RECV_BUFSZ=${CCICMP_RECV_BUFSZ}
  PUBLIC
    # Consumers of a shared build get dllimport; static builds get nothing.
    $<$<AND:$<BOOL:${BUILD_SHARED_LIBS}>,$<NOT:$<STREQUAL:$<TARGET_PROPERTY:ccsocket,TYPE>,SHARED_LIBRARY>>>:CCSOCKET_SHARED>
)

# -- Platform Linkage ---------------------------------------------------------
if(WIN32)
  # ws2_32 is auto-linked via #pragma comment(lib) on MSVC,
  # but MinGW / cross-compilation toolchains need the explicit link.
  target_link_libraries(ccsocket PUBLIC ws2_32)
endif()

# Solaris / illumos: socket functions live in libsocket, gethostbyname
# and related resolver functions live in libnsl.  Without these the
# linker will emit undefined references for socket(), sendfile(), etc.
if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  # Socket functions → libsocket, resolver → libnsl, sendfile → libsendfile.
  # On Solaris 11.4+ sendfile may also reside in libc; linking extra is harmless.
  target_link_libraries(ccsocket PUBLIC socket nsl sendfile)
endif()

# -- Alias for convenience ----------------------------------------------------
add_library(ccsocket::ccsocket ALIAS ccsocket)

# -- Tests --------------------------------------------------------------------
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
