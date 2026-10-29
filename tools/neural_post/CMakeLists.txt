cmake_minimum_required(VERSION 3.25)
project(PenumbraNeuralHost LANGUAGES CXX RC)
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "PenumbraNeuralHost requires an x64 build (-A x64)")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
execute_process(COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
    "${CMAKE_CURRENT_SOURCE_DIR}/../Write-PenumbraVrVersion.ps1"
    -OutputDirectory "${CMAKE_CURRENT_BINARY_DIR}/version" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../release.json")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/neural-sdk/ngx")
file(DOWNLOAD
    "https://raw.githubusercontent.com/NVIDIA/DLSS/9981c6350dc58c0ce21f024fd65e31e851c31689/include/nvsdk_ngx_defs.h"
    "${CMAKE_BINARY_DIR}/neural-sdk/ngx/nvsdk_ngx_defs.h"
    EXPECTED_HASH SHA256=ea23f33497cd274860d1c25a97644fce807dcb0037c594547203343103fad03e
    TLS_VERIFY ON STATUS ngx_download)
list(GET ngx_download 0 ngx_status)
if(NOT ngx_status EQUAL 0)
    message(FATAL_ERROR "Pinned NVIDIA definitions unavailable: ${ngx_download}")
endif()
add_executable(PenumbraNeuralHost PenumbraNeuralHost.cpp src/NeuralPass.cpp "${CMAKE_CURRENT_BINARY_DIR}/version/pvr_version.rc")
add_library(PenumbraNeuralForwarder SHARED vendor/nr-bridge/nr_forwarder.cpp "${CMAKE_CURRENT_BINARY_DIR}/version/pvr_version.rc")
set_target_properties(PenumbraNeuralForwarder PROPERTIES PREFIX "" OUTPUT_NAME "nvngx.dll_nr_bridge")
target_include_directories(PenumbraNeuralHost PRIVATE src
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/graphics" "${CMAKE_BINARY_DIR}/neural-sdk")
target_compile_definitions(PenumbraNeuralHost PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(PenumbraNeuralHost PRIVATE d3d11 d3d12 dxgi d3dcompiler advapi32)
