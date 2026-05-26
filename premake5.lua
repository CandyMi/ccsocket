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

-- ── Helpers ──────────────────────────────────────────────

local function cc_build(name, kind_, src, linklibs, targetname_, depends)
  project(name)
    kind(kind_)
    language("C")
    files(src)

    if targetname_ then
      targetname(targetname_)
    end

    if linklibs then
      links(linklibs)
    end

    if depends then
      dependson(depends)
    end

    filter "system:linux or bsd or macosx"
      pic "On"

    filter "system:solaris"
      pic "On"
      links { "socket", "sendfile" }

    filter {}
end

-- ── Source sets ──────────────────────────────────────────

local headers = { "ccsocket.h" }
local sources = { "ccsocket.c" }
local liblinks = {}

-- OpenSSL (optional)
if _OPTIONS["WITH_OSSL"] then
  table.insert(sources, "cctls.c")
  table.insert(headers, "cctls.h")
  table.insert(liblinks, "ssl")
  table.insert(liblinks, "crypto")
end

-- ICMP module (default on)
if _OPTIONS["WITH_ICMP"] ~= "off" then
  table.insert(sources, "ccicmp.c")
  table.insert(headers, "ccicmp.h")
end

-- ── Targets ─────────────────────────────────────────────

cc_build("ccsocket-static", "StaticLib",
  sources, liblinks, "ccsocket")

cc_build("ccsocket-dynamic", "SharedLib",
  sources, liblinks, "ccsocket",
  { "ccsocket-static" })

cc_build("testmain", "ConsoleApp",
  { "main.c" }, { "ccsocket" }, "main",
  { "ccsocket-static", "ccsocket-dynamic" })
