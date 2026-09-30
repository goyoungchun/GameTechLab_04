-- 저장소 루트: premake5.lua

workspace "JungleEngine"
    location "."
    configurations { "Debug", "Release", "ObjViewerDebug"}
    platforms { "x64" }
    startproject "EngineApp"

    system "windows"
    systemversion "latest"
    architecture "x86_64"

    language "C++"
    cppdialect "C++20"
    characterset "Unicode"
    staticruntime "Off"
    warnings "Extra"

    -- 실행 파일/라이브러리와 중간 파일을 분리
    targetdir "bin/%{cfg.platform}/%{cfg.buildcfg}"
    objdir "bin-int/%{prj.name}/%{cfg.platform}/%{cfg.buildcfg}"

    filter "configurations:Debug"
        defines { "_DEBUG" }
        runtime "Debug"
        symbols "On"
        optimize "Off"

    filter "configurations:Release"
        defines { "NDEBUG" }
        runtime "Release"
        symbols "On"
        optimize "Speed"
        
    filter "configurations:ObjViewerDebug"
        defines { "_DEBUG", "IS_OBJ_VIEWER=1" }
        runtime "Debug"
        symbols "On"
        optimize "Off"

    filter {}

project "EngineLib"
    location "EngineLib"
    kind "StaticLib"
    rtti "Off"

    files {
        "EngineLib/**.h",
        "EngineLib/**.hpp",
        "EngineLib/**.inl",
        "EngineLib/**.cpp",
        "EngineLib/Shaders/**.hlsl"
    }

    includedirs { "EngineLib" }

    -- 현재 엔진은 실행 중에 HLSL을 컴파일
    filter "files:**.hlsl"
        buildaction "None"
    filter {}

project "EngineApp"
    location "EngineApp"
    kind "WindowedApp"

    files {
        "EngineApp/App/**.h",
        "EngineApp/App/**.cpp"
    }

    includedirs { "EngineLib" }
    links { "EngineLib" }

    -- Assets/, Shaders/ 상대 경로의 기준
    debugdir "EngineLib"

project "UnitTest"
    location "UnitTest"
    kind "ConsoleApp"

    files {
        "UnitTest/**.h",
        "UnitTest/**.cpp"
    }

    includedirs { "EngineLib", "UnitTest" }
    links { "EngineLib" }

    pchheader "pch.h"
    pchsource "UnitTest/pch.cpp"

    nuget {
        "Microsoft.googletest.v140.windesktop.msvcstl.static.rt-dyn:1.8.1.8"
    }

    debugdir "EngineLib"
