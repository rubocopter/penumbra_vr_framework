cmake_minimum_required(VERSION 3.25)

project(PenumbraVR VERSION 0.0.1 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The initial binary probe targets the Windows releases of Penumbra.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Configure this project for Win32. The observed Penumbra executables are x86.")
endif()

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

function(pvr_enable_warnings target)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8)
endfunction()

add_library(pvr_build_catalog STATIC
    src/common/build_catalog.cpp
)
target_include_directories(pvr_build_catalog PUBLIC include)
target_link_libraries(pvr_build_catalog PUBLIC bcrypt)
pvr_enable_warnings(pvr_build_catalog)

add_library(pvr_sdl_frame_hook STATIC
    src/hooks/sdl_frame_hook.cpp
)
target_include_directories(pvr_sdl_frame_hook PUBLIC src/hooks)
pvr_enable_warnings(pvr_sdl_frame_hook)

add_library(pvr_iat_hook STATIC
    src/hooks/iat_hook.cpp
)
target_include_directories(pvr_iat_hook PUBLIC src/hooks)
pvr_enable_warnings(pvr_iat_hook)

target_link_libraries(pvr_sdl_frame_hook PUBLIC pvr_iat_hook)

add_library(pvr_opengl_telemetry STATIC
    src/hooks/opengl_matrix_telemetry.cpp
)
target_include_directories(pvr_opengl_telemetry PUBLIC src/hooks)
target_link_libraries(pvr_opengl_telemetry PUBLIC pvr_iat_hook)
pvr_enable_warnings(pvr_opengl_telemetry)

add_library(pvr_black_plague_probe SHARED
    src/probe/black_plague_probe.cpp
    src/probe/black_plague_probe.def
    src/probe/log.cpp
)
target_include_directories(pvr_black_plague_probe PRIVATE src/probe)
target_link_libraries(
    pvr_black_plague_probe
    PRIVATE pvr_build_catalog pvr_sdl_frame_hook pvr_opengl_telemetry)
set_target_properties(pvr_black_plague_probe PROPERTIES
    OUTPUT_NAME "PenumbraVR.BlackPlague.Probe"
)
pvr_enable_warnings(pvr_black_plague_probe)

add_executable(pvr_probe_launcher
    src/launcher/main.cpp
    src/launcher/pe_memory_inspector.cpp
)
target_link_libraries(pvr_probe_launcher PRIVATE pvr_build_catalog)
set_target_properties(pvr_probe_launcher PROPERTIES
    OUTPUT_NAME "PenumbraVR.ProbeLauncher"
)
pvr_enable_warnings(pvr_probe_launcher)

if(BUILD_TESTING)
    add_library(pvr_test_sdl SHARED
        tests/frame_hook/sdl_stub.cpp
        tests/frame_hook/sdl_stub.def
    )
    set_target_properties(pvr_test_sdl PROPERTIES OUTPUT_NAME "SDL")
    pvr_enable_warnings(pvr_test_sdl)

    add_executable(pvr_frame_hook_test
        tests/frame_hook/frame_hook_test.cpp
    )
    target_link_libraries(pvr_frame_hook_test PRIVATE pvr_sdl_frame_hook pvr_test_sdl)
    pvr_enable_warnings(pvr_frame_hook_test)

    add_test(NAME frame_hook COMMAND pvr_frame_hook_test)

    add_library(pvr_test_opengl SHARED
        tests/opengl_telemetry/opengl_stub.cpp
        tests/opengl_telemetry/opengl_stub.def
    )
    set_target_properties(pvr_test_opengl PROPERTIES OUTPUT_NAME "OPENGL32")
    pvr_enable_warnings(pvr_test_opengl)

    add_executable(pvr_opengl_telemetry_test
        tests/opengl_telemetry/opengl_telemetry_test.cpp
    )
    target_link_libraries(
        pvr_opengl_telemetry_test PRIVATE pvr_opengl_telemetry pvr_test_opengl)
    pvr_enable_warnings(pvr_opengl_telemetry_test)

    add_test(NAME opengl_telemetry COMMAND pvr_opengl_telemetry_test)
endif()
