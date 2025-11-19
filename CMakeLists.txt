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

MESSAGE("-- Detecting OS: '${CMAKE_SYSTEM_NAME}'")

file(GLOB ccsocket_src ccsocket.c)

if(MSVC)
  add_compile_options("/source-charset:utf-8")
endif()

add_executable(main main.c)
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  # MESSAGE("Windows")
  add_library(ccsocket_object OBJECT ${ccsocket_src})
  add_library(ccsocket-dynamic SHARED $<TARGET_OBJECTS:ccsocket_object>)
  set_target_properties(ccsocket-dynamic PROPERTIES OUTPUT_NAME "ccsocket" PREFIX "" )
  # add_library(ccsocket-static STATIC $<TARGET_OBJECTS:ccsocket_object>)
  # set_target_properties(ccsocket-static PROPERTIES OUTPUT_NAME "ccsocket" PREFIX "")
  target_link_libraries(main PRIVATE ccsocket-dynamic)
else()
  # MESSAGE("Unix-Like")
  add_library(ccsocket_object OBJECT ${ccsocket_src})
  target_compile_options(ccsocket_object PRIVATE -fPIC)
  add_library(ccsocket-dynamic SHARED $<TARGET_OBJECTS:ccsocket_object>)
  set_target_properties(ccsocket-dynamic PROPERTIES OUTPUT_NAME "ccsocket" PREFIX "lib" )
  add_library(ccsocket-static STATIC $<TARGET_OBJECTS:ccsocket_object>)
  set_target_properties(ccsocket-static PROPERTIES OUTPUT_NAME "ccsocket" PREFIX "lib")
  target_link_libraries(main PRIVATE ccsocket-static)
endif()
