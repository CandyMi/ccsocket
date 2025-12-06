---@diagnostic disable: undefined-global, undefined-field

workspace "ccsocket"
  configurations { "Debug", "Release" }

local function ccbuild (
  project_name, project_type,
  project_language, project_files,
  project_links, project_filename,
  preject_output
)
  project(project_name)
    kind(project_type)
    language(project_language)
    files(project_files)

    targetdir "build"
    if preject_output then
      location(preject_output)
      targetdir(preject_output)
    end

    if project_links then
      links(project_links)
      libdirs({ path.getabsolute(preject_output), '/usr/local/lib' })
    end

    if project_filename then
      targetname(project_filename)
    end

    filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"
      optimize "Off"

    filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

    filter { "system:windows" }
      targetprefix ""

    filter { "system:macosx" }
      runpathdirs { path.getabsolute(preject_output) }
end

ccbuild(
  'ccsocket-dynamic', 'SharedLib', 'C',
  {'ccsocket.c', 'ccsocket.h'},
  nil, 'ccsocket', 'build'
)

ccbuild(
  'ccsocket-static', 'StaticLib', 'C',
  {'ccsocket.c', 'ccsocket.h'},
  nil, 'ccsocket', 'build'
)

ccbuild(
  'testmain', 'ConsoleApp', 'C',
  {'main.c', 'ccsocket.h'},
  {'ccsocket'}, 'main', 'build'
)