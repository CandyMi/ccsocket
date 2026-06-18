# ============================================================================
# libccsocket — Cross-Platform Socket Abstraction Library
# https://agents.md/  |  MIT License  |  Author: CandyMi
# ============================================================================

cmake_minimum_required(VERSION 3.0...3.31)

# Honor visibility properties (C_VISIBILITY_PRESET) for all target types,
# including STATIC_LIBRARY and EXECUTABLE.  Available since CMake 3.3.
if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW)
endif()

# -- Project ------------------------------------------------------------------
project(libccsocket
  VERSION     1.0.0
  DESCRIPTION "Cross-platform C socket abstraction library (TCP/UDP/ICMP/Unix)"
  LANGUAGES   C
)

# -- C Standard ---------------------------------------------------------------
set(CMAKE_C_STANDARD          99)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_C_EXTENSIONS        OFF)

# -- Build RPATH (so tests find the shared library at runtime) ----------------
set(CMAKE_BUILD_RPATH ${CMAKE_BINARY_DIR})

# -- Build Type Default -------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# -- Options ------------------------------------------------------------------
option(BUILD_SHARED_LIBS "Let ccsocket alias point to the shared library" ON)
option(BUILD_TESTING     "Build test suite"                               ON)

# Numeric compile-time configuration (CACHE STRING, NOT option)
set(CCICMP_MAX_PAYLOAD 65500 CACHE STRING "Max ICMP payload bytes")
set(CCICMP_RECV_BUFSZ  65535 CACHE STRING "ICMP receive buffer size")

# -- Compile Warnings ---------------------------------------------------------
set(CC_WARNINGS "")
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  set(CC_WARNINGS
    -Wall -Wextra -Wpedantic -Wshadow
    -Wstrict-prototypes -Wold-style-definition
    -Wmissing-prototypes -Wmissing-declarations
    -Werror=implicit-function-declaration -Werror=return-type
  )
elseif(MSVC)
  set(CC_WARNINGS /W4 /utf-8)
endif()

# -- Sources ------------------------------------------------------------------
set(CCSOCKET_SOURCES
  ccsocket.c
  ccicmp.c
  ccdns.c
)
set(CCSOCKET_HEADERS
  ccsocket.h
  ccicmp.h
  ccdns.h
)

# -- Object library (compiled once with PIC) ----------------------------------
add_library(ccsocket_obj OBJECT ${CCSOCKET_SOURCES} ${CCSOCKET_HEADERS})

target_include_directories(ccsocket_obj
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(ccsocket_obj
  PRIVATE
    CCICMP_MAX_PAYLOAD=${CCICMP_MAX_PAYLOAD}
    CCICMP_RECV_BUFSZ=${CCICMP_RECV_BUFSZ}
)

target_compile_options(ccsocket_obj PRIVATE ${CC_WARNINGS})

# Symbol visibility: hide everything except CCSOCKET_EXPORT symbols.
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set_target_properties(ccsocket_obj PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

# -- Shared library -----------------------------------------------------------
add_library(ccsocket_shared SHARED)

target_link_libraries(ccsocket_shared PUBLIC ccsocket_obj)

target_compile_definitions(ccsocket_shared
  PRIVATE   CCSOCKET_BUILD_SHARED   # dllexport when building
  PUBLIC    CCSOCKET_SHARED         # dllimport when consuming
)

set_target_properties(ccsocket_shared PROPERTIES
  OUTPUT_NAME ccsocket
)

# -- Static library -----------------------------------------------------------
add_library(ccsocket_static STATIC)

target_link_libraries(ccsocket_static PUBLIC ccsocket_obj)

set_target_properties(ccsocket_static PROPERTIES
  OUTPUT_NAME ccsocket
)

# -- Platform Linkage (applied to both via the object library) ----------------
if(WIN32)
  target_link_libraries(ccsocket_obj PUBLIC ws2_32)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  target_link_libraries(ccsocket_obj PUBLIC socket nsl sendfile)
endif()

# -- Consumer alias (shared or static depending on BUILD_SHARED_LIBS) ---------
add_library(ccsocket INTERFACE)
if(BUILD_SHARED_LIBS)
  target_link_libraries(ccsocket INTERFACE ccsocket_shared)
else()
  target_link_libraries(ccsocket INTERFACE ccsocket_static)
endif()
add_library(ccsocket::ccsocket ALIAS ccsocket)

# -- Tests --------------------------------------------------------------------
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

# -- Documentation (Doxygen) --------------------------------------------------
option(BUILD_DOCS "Build Doxygen documentation (requires Doxygen)" OFF)

if(BUILD_DOCS)
  find_package(Doxygen REQUIRED)

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    @ONLY
  )

  add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    COMMENT "Generating API documentation (Doxygen) — open ${CMAKE_CURRENT_BINARY_DIR}/docs/html/index.html"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  )
endif()
