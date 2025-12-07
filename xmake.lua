---@diagnostic disable: undefined-global, undefined-field

add_rules("mode.debug", "mode.release")

local stdc = "c99"
local stdcxx = "c++11"
local output = "build"

-- 生成.o文件
target("ccsocket-object")
    set_kind("object")
    add_files("ccsocket.c")
    set_languages(stdc)
    set_targetdir(output)

-- 动态库
target("ccsocket-dynamic")
    set_kind("shared")
    add_deps("ccsocket-object")
    set_targetdir(output)
    set_basename("ccsocket")

-- 静态库
target("ccsocket-static")
    set_kind("static")
    add_deps("ccsocket-object")
    set_targetdir(output)
    set_basename("ccsocket")

-- 测试文件
target("testmain")
    set_kind("binary")
    add_files("main.c")
    add_deps("ccsocket-dynamic", "ccsocket-static")
    add_links('ccsocket')
    add_rpathdirs(output)
    add_linkdirs(output)
    set_targetdir(output)
    set_basename("main")