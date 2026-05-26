---@diagnostic disable: undefined-global, undefined-field

workspace "ccsocket"
  configurations { "Debug", "Release" }
  characterset "ASCII"

  platforms { "x64", "Win32", "ARM64" }

  startproject "testmain"

  filter "system:windows"
    targetprefix ""
    buildoptions { "/source-charset:utf-8" }

  filter "configurations:Debug"
    defines { "DEBUG" }
    symbols "On"
    optimize "Off"

  filter "configurations:Release"
    defines { "NDEBUG" }
    optimize "On"

  filter "toolset:msc*"
    buildoptions { "/source-charset:utf-8" }

  filter { "toolset:not msc*" }
    buildoptions { "-std=c99" }

-- ── Options ──────────────────────────────────────────────

newoption {
  trigger = "WITH_OSSL",
  description = "Build with OpenSSL (TLS support)",
}

newoption {
  trigger = "WITH_ICMP",
  description = "Build with ICMP echo/reply module",
}

-- ── Source sets ──────────────────────────────────────────

local lib_sources = { "ccsocket.c", "ccsocket.h" }
local lib_links   = {}

if _OPTIONS["WITH_OSSL"] then
  table.insert(lib_sources, "cctls.c")
  table.insert(lib_sources, "cctls.h")
  table.insert(lib_links,   "ssl")
  table.insert(lib_links,   "crypto")
end

if _OPTIONS["WITH_ICMP"] ~= "off" then
  table.insert(lib_sources, "ccicmp.c")
  table.insert(lib_sources, "ccicmp.h")
end

-- c-source files only (strip .h for object-only builds)
local lib_csrc = {}
for _, f in ipairs(lib_sources) do
  if f:match("%.c$") then
    table.insert(lib_csrc, f)
  end
end

-- testmain sources: main.c + all library .c files (linked as objects)
local test_sources = { "main.c" }
for _, f in ipairs(lib_csrc) do
  table.insert(test_sources, f)
end

-- ── Targets ─────────────────────────────────────────────

-- static library (no PIC)
project "ccsocket-static"
  kind "StaticLib"
  language "C"
  files(lib_sources)
  targetdir "build"
  objdir "build/obj/static"
  targetname "ccsocket"
  links(lib_links)

  filter "system:linux or bsd or macosx"
    pic "Off"

  filter "system:solaris"
    links { "socket", "sendfile" }

  filter {}

-- shared library (PIC)
project "ccsocket-dynamic"
  kind "SharedLib"
  language "C"
  files(lib_sources)
  targetdir "build"
  objdir "build/obj/dynamic"
  targetname "ccsocket"
  links(lib_links)

  filter "system:linux or bsd or macosx"
    pic "On"

  filter "system:solaris"
    pic "On"
    links { "socket", "sendfile" }

  filter {}

-- test executable (links library .o files directly, no .a or .so dependency)
project "testmain"
  kind "ConsoleApp"
  language "C"
  files(test_sources)
  targetdir "build"
  objdir "build/obj/testmain"
  targetname "main"

  filter "system:solaris"
    links { "socket", "sendfile" }

  filter {}
