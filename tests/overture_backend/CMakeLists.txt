cmake_minimum_required(VERSION 3.25)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(OvertureSourceConsumer LANGUAGES CXX)
    enable_testing()
endif()
get_filename_component(PVR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(NOT TARGET pvr_overture_backend)
    add_library(pvr_overture_backend STATIC
        "${PVR_ROOT}/src/backends/overture/overture_backend.cpp"
        "${PVR_ROOT}/src/runtime/vr_math.cpp"
        "${PVR_ROOT}/src/runtime/vr_tracking_space.cpp"
        "${PVR_ROOT}/src/runtime/vr_locomotion.cpp"
        "${PVR_ROOT}/src/runtime/vr_play_mode_policy.cpp"
        "${PVR_ROOT}/src/runtime/vr_settings.cpp"
        "${PVR_ROOT}/src/runtime/vr_input_state.cpp"
        "${PVR_ROOT}/src/runtime/vr_native_intents.cpp")
    target_include_directories(pvr_overture_backend PUBLIC
        "${PVR_ROOT}/src/backends/overture" "${PVR_ROOT}/src/runtime")
    target_compile_features(pvr_overture_backend PUBLIC cxx_std_20)
endif()
add_executable(pvr_overture_source_integration_test
    source_integration_test.cpp
    "${PVR_ROOT}/src/adapters/overture_source/overture_source_integration.cpp"
    "${PVR_ROOT}/products/overture/HPL1Engine/sources/math/Math.cpp"
    "${PVR_ROOT}/products/overture/HPL1Engine/sources/math/MathTypes.cpp"
    "${PVR_ROOT}/products/overture/HPL1Engine/sources/math/Quaternion.cpp"
    "${PVR_ROOT}/products/overture/HPL1Engine/sources/math/CRC.cpp"
    "${PVR_ROOT}/products/overture/HPL1Engine/sources/graphics/Color.cpp"
    "${PVR_ROOT}/products/overture/tests/VRTrackingTest/LowLevelStubs.cpp")
target_include_directories(pvr_overture_source_integration_test PRIVATE
    shim "${PVR_ROOT}/src/adapters/overture_source"
    "${PVR_ROOT}/products/overture/HPL1Engine/include"
    "${PVR_ROOT}/products/overture/dependencies/openvr-2.15.6/headers")
target_compile_definitions(pvr_overture_source_integration_test PRIVATE NOMINMAX)
target_compile_features(pvr_overture_source_integration_test PRIVATE cxx_std_20)
target_link_libraries(pvr_overture_source_integration_test PRIVATE pvr_overture_backend)
add_test(NAME overture_source_integration COMMAND pvr_overture_source_integration_test)
