---@diagnostic disable: undefined-global, undefined-field

add_rules("mode.debug", "mode.release")

-- 生成.o文件
target("ccsocket-object")
    set_kind("object")
    add_files("ccsocket.c")
    set_languages('c89')
    set_targetdir("build")

-- 动态库
target("ccsocket-dynamic")
    set_kind("shared")
    add_deps("ccsocket-object")
    set_targetdir("build")
    set_basename("ccsocket")

-- 静态库
target("ccsocket-static")
    set_kind("static")
    add_deps("ccsocket-object")
    set_targetdir("build")
    set_basename("ccsocket")

-- 测试文件
target("testmain")
    set_kind("binary")
    add_files("main.c")
    add_deps("ccsocket-dynamic", "ccsocket-static")
    add_rpathdirs("build")
    add_linkdirs("build")
    add_links('ccsocket')
    set_targetdir("build")
    set_basename("main")