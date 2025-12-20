message("======================================")
message("Project Name   : ccsocket")
message("Author  Name   : CandyMi")
message("Author  Email  : 869646063@qq.com")
message("Author  Github : github.com/CandyMi")
message("======================================")

# 最低版本号
cmake_minimum_required(VERSION 2.8...3.13)

# 项目名称
project("ccsocket")

message("-- Detecting platform: '${CMAKE_SYSTEM_NAME}'")
message("-- Detecting install prefix path: '${CMAKE_INSTALL_PREFIX}'")

set(ccsocket_headers ccsocket.h)
set(ccsocket_source ccsocket.c)
set(ccsocket_testmain main.c)

if(MSVC)
  add_compile_options("/source-charset:utf-8")
else()
  set(CMAKE_C_STANDARD 99)
  set(CMAKE_C_EXTENSIONS ON)
  set(CMAKE_C_STANDARD_REQUIRED ON)
  option(USE_CXX "force use c++ compiler" OFF)
  if (USE_CXX) #如果强制使用C++编译.
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_EXTENSIONS ON)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set_source_files_properties(
      ${ccsocket_headers} ${ccsocket_source} ${ccsocket_testmain}
      PROPERTIES
        LANGUAGE CXX
    )
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Clang 编译器会有警告
      add_compile_options(-Wno-deprecated)
    endif()
  endif()
endif()

# option(WITH_ICMP "ccsocket with icmp" ON)
# if (WITH_ICMP)
#   message("-- Compiler with 'ICMP'.")
#   set(ccsocket_source ${ccsocket_source} ccicmp.c)
# endif (WITH_ICMP)

option(WITH_OSSL "ccsocket with tls" ON)
if (WITH_OSSL)
  message("-- Compiler with 'OpenSSL'.")
  set(ccsocket_source ${ccsocket_source} cctls.c)
  if (NOT BSD)
    if(EXISTS "/etc/centos-release") # centos only dynamic library ?
      set(OPENSSL_USE_STATIC_LIBS OFF)
      set(OPENSSL_MSVC_STATIC_RT OFF)
    else()
      set(OPENSSL_USE_STATIC_LIBS ON)
      set(OPENSSL_MSVC_STATIC_RT ON)
    endif()
  endif(NOT BSD)
  if (NOT WIN32)
    include_directories(/usr/local/include)
    link_directories(/usr/lib64)
  endif()
  find_package(OpenSSL 1.1.1 REQUIRED)
  include_directories(${OPENSSL_INCLUDE_DIR})
  message("-- OpenSSL SSL PATH: ${OPENSSL_SSL_LIBRARIES}")
  message("-- OpenSSL Crypto PATH: ${OPENSSL_CRYPTO_LIBRARIES}")
endif(WITH_OSSL)

add_executable(main ${ccsocket_testmain})
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  # message("Windows")
  add_library(ccsocket_object OBJECT ${ccsocket_source})
  add_library(ccsocket-dynamic SHARED $<TARGET_OBJECTS:ccsocket_object>)
  set_target_properties(ccsocket-dynamic PROPERTIES OUTPUT_NAME "ccsocket" PREFIX "" PUBLIC_HEADER "${ccsocket_headers}")
  add_library(ccsocket-static STATIC $<TARGET_OBJECTS:ccsocket_object>)
  set_target_properties(ccsocket-static PROPERTIES OUTPUT_NAME "ccsocket" PREFIX "" PUBLIC_HEADER "${ccsocket_headers}")
  target_link_libraries(main PRIVATE ccsocket-dynamic)
else()
  # message("Unix-Like")
  add_library(ccsocket_object OBJECT ${ccsocket_source})
  target_compile_options(ccsocket_object PRIVATE -fPIC)
  add_library(ccsocket-dynamic SHARED $<TARGET_OBJECTS:ccsocket_object>)
  set_target_properties(ccsocket-dynamic PROPERTIES OUTPUT_NAME "ccsocket" PREFIX "lib" PUBLIC_HEADER "${ccsocket_headers}")
  add_library(ccsocket-static STATIC $<TARGET_OBJECTS:ccsocket_object>)
  set_target_properties(ccsocket-static PROPERTIES OUTPUT_NAME "ccsocket" PREFIX "lib" PUBLIC_HEADER "${ccsocket_headers}")
  if (${CMAKE_SYSTEM_NAME} MATCHES "SunOS") # Solaris的部分衍生/分叉需要单独链接文件
    find_library(SOCKET_PATH socket)
    find_library(SENDFILE_PATH sendfile)
    target_link_libraries(ccsocket-dynamic PRIVATE ${SOCKET_PATH} ${SENDFILE_PATH})
    target_link_libraries(ccsocket-static PRIVATE ${SOCKET_PATH} ${SENDFILE_PATH})
  endif()
  target_link_libraries(main PRIVATE ccsocket-static)
endif()

if (WITH_OSSL)
  target_link_libraries(ccsocket-dynamic PRIVATE ${OPENSSL_SSL_LIBRARIES} ${OPENSSL_CRYPTO_LIBRARIES})
  target_link_libraries(ccsocket-static PRIVATE ${OPENSSL_SSL_LIBRARIES} ${OPENSSL_CRYPTO_LIBRARIES})
endif(WITH_OSSL)

install(
  TARGETS ccsocket-dynamic ccsocket-static
  RUNTIME  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
  ARCHIVE  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
  LIBRARY  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include # includes
)
