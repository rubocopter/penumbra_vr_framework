cmake_minimum_required(VERSION 3.25)

project(PenumbraVR VERSION 0.0.1 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The initial binary probe targets the Windows releases of Penumbra.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Configure this project for Win32. The observed Penumbra executables are x86.")
endif()

include(CTest)

set(PENUMBRA_VR_OPENVR_SDK "" CACHE PATH
    "Optional OpenVR SDK root containing headers/openvr.h and bin/win32/openvr_api.dll")

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

add_library(pvr_rel32_call_hook STATIC
    src/hooks/rel32_call_hook.cpp
)
target_include_directories(pvr_rel32_call_hook PUBLIC src/hooks)
pvr_enable_warnings(pvr_rel32_call_hook)

add_library(pvr_opengl_eye_targets STATIC
    src/graphics/opengl_eye_targets.cpp
    src/graphics/opengl_menu_frame.cpp
)
target_include_directories(pvr_opengl_eye_targets PUBLIC src/graphics)
target_link_libraries(pvr_opengl_eye_targets PUBLIC opengl32 pvr_vr_math)
pvr_enable_warnings(pvr_opengl_eye_targets)

add_library(pvr_visual_calibration STATIC
    src/graphics/visual_calibration.cpp
)
target_include_directories(pvr_visual_calibration PUBLIC src/graphics)
pvr_enable_warnings(pvr_visual_calibration)

add_library(pvr_opengl_eye_scissor STATIC src/hooks/opengl_eye_scissor.cpp)
target_include_directories(pvr_opengl_eye_scissor PUBLIC src/hooks)
target_link_libraries(pvr_opengl_eye_scissor PUBLIC pvr_iat_hook opengl32)
pvr_enable_warnings(pvr_opengl_eye_scissor)

add_library(pvr_spatial_audio STATIC
    src/audio/spatial_audio.cpp
)
target_include_directories(pvr_spatial_audio PUBLIC src/audio)
pvr_enable_warnings(pvr_spatial_audio)

add_library(pvr_vr_math STATIC
    src/runtime/vr_math.cpp
    src/runtime/vr_grab_pose.cpp
    src/runtime/vr_hand_pose.cpp
    src/runtime/vr_interaction_policy.cpp
    src/runtime/vr_locomotion.cpp
    src/runtime/vr_tracking_space.cpp
)
target_include_directories(pvr_vr_math PUBLIC src/runtime)
pvr_enable_warnings(pvr_vr_math)

add_library(pvr_stereo_render_policy STATIC
    src/runtime/stereo_render_policy.cpp
)
target_include_directories(pvr_stereo_render_policy PUBLIC src/runtime)
pvr_enable_warnings(pvr_stereo_render_policy)

add_library(pvr_vr_settings STATIC
    src/runtime/vr_settings.cpp
    src/runtime/vr_settings_editor.cpp
)
target_include_directories(pvr_vr_settings PUBLIC src/runtime)
pvr_enable_warnings(pvr_vr_settings)

add_library(pvr_vr_settings_store STATIC
    src/launcher/vr_settings_store.cpp
)
target_include_directories(pvr_vr_settings_store PUBLIC src/launcher)
target_link_libraries(pvr_vr_settings_store PUBLIC pvr_vr_settings)
pvr_enable_warnings(pvr_vr_settings_store)

add_library(pvr_black_plague_settings STATIC
    src/backends/black_plague/vr_settings_capabilities.cpp
)
target_include_directories(pvr_black_plague_settings PUBLIC
    src/backends/black_plague src/runtime)
target_link_libraries(pvr_black_plague_settings PUBLIC pvr_vr_settings)
pvr_enable_warnings(pvr_black_plague_settings)

add_library(pvr_vr_input_state STATIC
    src/runtime/vr_input_state.cpp
)
target_include_directories(pvr_vr_input_state PUBLIC src/runtime)
target_link_libraries(pvr_vr_input_state PUBLIC pvr_vr_settings)
pvr_enable_warnings(pvr_vr_input_state)

add_library(pvr_vr_action_input STATIC src/runtime/vr_action_input.cpp)
target_include_directories(pvr_vr_action_input PUBLIC src/runtime)
target_link_libraries(pvr_vr_action_input PUBLIC pvr_vr_input_state)
pvr_enable_warnings(pvr_vr_action_input)

add_library(pvr_vr_native_intents STATIC src/runtime/vr_native_intents.cpp)
target_include_directories(pvr_vr_native_intents PUBLIC src/runtime)
target_link_libraries(pvr_vr_native_intents PUBLIC pvr_vr_input_state)
pvr_enable_warnings(pvr_vr_native_intents)

add_library(pvr_overture_backend STATIC
    src/backends/overture/overture_backend.cpp
)
target_include_directories(
    pvr_overture_backend
    PUBLIC src/backends/overture src/runtime)
target_link_libraries(
    pvr_overture_backend
    PUBLIC pvr_vr_math pvr_vr_native_intents pvr_vr_settings)
pvr_enable_warnings(pvr_overture_backend)

add_library(pvr_hpl1_camera_adapter STATIC
    src/adapters/hpl1/camera_matrix_override.cpp
)
target_include_directories(
    pvr_hpl1_camera_adapter
    PUBLIC src/adapters/hpl1 src/runtime)
target_link_libraries(pvr_hpl1_camera_adapter PUBLIC pvr_vr_math)
pvr_enable_warnings(pvr_hpl1_camera_adapter)

add_library(pvr_render_target_policy STATIC
    src/runtime/render_target_policy.cpp
)
target_include_directories(pvr_render_target_policy PUBLIC src/runtime)
target_link_libraries(pvr_render_target_policy PUBLIC pvr_vr_settings)
pvr_enable_warnings(pvr_render_target_policy)

add_library(pvr_pe_large_address STATIC
    src/deployment/pe_large_address.cpp
)
target_include_directories(pvr_pe_large_address PUBLIC src/deployment)
pvr_enable_warnings(pvr_pe_large_address)

set(PVR_OPENVR_LOADER "")
if(PENUMBRA_VR_OPENVR_SDK)
    set(PVR_OPENVR_HEADER "${PENUMBRA_VR_OPENVR_SDK}/headers/openvr.h")
    set(PVR_OPENVR_LOADER "${PENUMBRA_VR_OPENVR_SDK}/bin/win32/openvr_api.dll")
    if(NOT EXISTS "${PVR_OPENVR_HEADER}" OR NOT EXISTS "${PVR_OPENVR_LOADER}")
        message(FATAL_ERROR
            "PENUMBRA_VR_OPENVR_SDK must contain headers/openvr.h and bin/win32/openvr_api.dll")
    endif()
    add_library(pvr_openvr_session STATIC
        src/runtime/openvr_session.cpp
        src/runtime/openvr_controller_input.cpp
    )
    target_include_directories(
        pvr_openvr_session
        PUBLIC src/runtime
        PRIVATE "${PENUMBRA_VR_OPENVR_SDK}/headers")
    target_compile_definitions(pvr_openvr_session PUBLIC PVR_HAS_OPENVR=1)
else()
    add_library(pvr_openvr_session STATIC
        src/runtime/openvr_session_stub.cpp
    )
    target_include_directories(pvr_openvr_session PUBLIC src/runtime)
    target_compile_definitions(pvr_openvr_session PUBLIC PVR_HAS_OPENVR=0)
endif()
pvr_enable_warnings(pvr_openvr_session)
target_link_libraries(pvr_openvr_session PUBLIC pvr_vr_action_input)

add_library(pvr_black_plague_body_adapter STATIC
    src/backends/black_plague/body_reconciliation_shadow.cpp
    src/backends/black_plague/body_adapter_boundary.cpp
    src/backends/black_plague/black_plague_body_adapter.cpp
)
target_include_directories(pvr_black_plague_body_adapter PUBLIC
    src/backends/black_plague src/runtime)
target_link_libraries(pvr_black_plague_body_adapter PUBLIC pvr_vr_math)
pvr_enable_warnings(pvr_black_plague_body_adapter)

add_library(pvr_black_plague_backend_probe STATIC
    src/backends/black_plague/body_collision_probe.cpp
    src/backends/black_plague/movement_ownership_probe.cpp
    src/backends/black_plague/eye_target_probe.cpp
    src/backends/black_plague/render_world_probe.cpp
    src/backends/black_plague/native_input_bridge.cpp
    src/backends/black_plague/spatial_interaction.cpp
)
target_include_directories(
    pvr_black_plague_backend_probe
    PUBLIC src/backends/black_plague
    PRIVATE src/hooks)
target_link_libraries(
    pvr_black_plague_backend_probe
    PUBLIC
        pvr_rel32_call_hook
        pvr_opengl_eye_targets
        pvr_opengl_eye_scissor
        pvr_openvr_session
        pvr_hpl1_camera_adapter
        pvr_vr_math
        pvr_black_plague_body_adapter
        pvr_black_plague_settings
        pvr_stereo_render_policy)
target_link_libraries(pvr_black_plague_backend_probe PUBLIC pvr_vr_native_intents pvr_iat_hook)
target_link_libraries(pvr_black_plague_backend_probe PRIVATE opengl32)
pvr_enable_warnings(pvr_black_plague_backend_probe)

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
    PRIVATE
        pvr_build_catalog
        pvr_sdl_frame_hook
        pvr_opengl_telemetry
        pvr_openvr_session
        pvr_render_target_policy
        pvr_vr_settings_store
        pvr_black_plague_backend_probe)
set_target_properties(pvr_black_plague_probe PROPERTIES
    OUTPUT_NAME "PenumbraVR.BlackPlague.Probe"
)
pvr_enable_warnings(pvr_black_plague_probe)
add_custom_target(pvr_openvr_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/openvr"
        "$<TARGET_FILE_DIR:pvr_black_plague_probe>/vr"
    VERBATIM)
if(PVR_OPENVR_LOADER)
    add_custom_command(TARGET pvr_black_plague_probe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PVR_OPENVR_LOADER}"
            "$<TARGET_FILE_DIR:pvr_black_plague_probe>/openvr_api.dll"
        VERBATIM)
endif()

add_executable(pvr_probe_launcher
    src/launcher/main.cpp
    src/launcher/pe_memory_inspector.cpp
)
target_link_libraries(
    pvr_probe_launcher PRIVATE pvr_build_catalog pvr_vr_settings_store shell32)
set_target_properties(pvr_probe_launcher PROPERTIES
    OUTPUT_NAME "PenumbraVR.ProbeLauncher"
)
pvr_enable_warnings(pvr_probe_launcher)

find_program(PVR_POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
add_custom_target(pvr_overture_product
    COMMAND "${PVR_POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_SOURCE_DIR}/tools/Build-OvertureProduct.ps1"
        -Configuration "$<CONFIG>"
    USES_TERMINAL
    VERBATIM)
add_custom_target(pvr_overture_package
    COMMAND "${PVR_POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_SOURCE_DIR}/tools/Build-OvertureProduct.ps1"
        -Configuration "$<CONFIG>" -Package
    USES_TERMINAL
    VERBATIM)

if(BUILD_TESTING)
    add_executable(pvr_body_collision_probe_test
        tests/black_plague_body/body_collision_probe_test.cpp
        tests/black_plague_body/native_movement_boundary_stub.cpp)
    target_include_directories(pvr_body_collision_probe_test PRIVATE
        src/hooks src/backends/black_plague src/runtime)
    target_link_libraries(pvr_body_collision_probe_test PRIVATE
        pvr_rel32_call_hook pvr_vr_math pvr_black_plague_body_adapter)
    pvr_enable_warnings(pvr_body_collision_probe_test)
    add_test(NAME body_collision_probe COMMAND pvr_body_collision_probe_test)

    add_executable(pvr_black_plague_body_adapter_test
        tests/black_plague_body/black_plague_body_adapter_test.cpp
        src/backends/black_plague/body_adapter_boundary.cpp)
    target_include_directories(pvr_black_plague_body_adapter_test PRIVATE
        src/backends/black_plague)
    pvr_enable_warnings(pvr_black_plague_body_adapter_test)
    add_test(NAME black_plague_body_adapter COMMAND pvr_black_plague_body_adapter_test)

    add_executable(pvr_body_reconciliation_test
        tests/vr_tracking_space/body_reconciliation_test.cpp
        src/backends/black_plague/body_reconciliation_shadow.cpp)
    target_include_directories(pvr_body_reconciliation_test PRIVATE
        src/backends/black_plague)
    target_link_libraries(pvr_body_reconciliation_test PRIVATE pvr_vr_math)
    pvr_enable_warnings(pvr_body_reconciliation_test)
    add_test(NAME body_reconciliation COMMAND pvr_body_reconciliation_test)

    add_executable(pvr_spatial_interaction_test tests/vr_input_state/spatial_interaction_test.cpp)
    target_include_directories(pvr_spatial_interaction_test PRIVATE src/hooks src/backends/black_plague src/adapters/hpl1 src/graphics)
    target_link_libraries(pvr_spatial_interaction_test PRIVATE pvr_vr_math pvr_iat_hook pvr_rel32_call_hook)
    pvr_enable_warnings(pvr_spatial_interaction_test)
    add_test(NAME spatial_interaction COMMAND pvr_spatial_interaction_test)
    add_executable(pvr_vr_grab_pose_test tests/vr_math/vr_grab_pose_test.cpp)
    target_link_libraries(pvr_vr_grab_pose_test PRIVATE pvr_vr_math)
    pvr_enable_warnings(pvr_vr_grab_pose_test)
    add_test(NAME vr_grab_pose COMMAND pvr_vr_grab_pose_test)
    add_executable(pvr_vr_native_intents_test tests/vr_input_state/vr_native_intents_test.cpp)
    target_include_directories(pvr_vr_native_intents_test PRIVATE src/adapters/hpl1)
    target_link_libraries(pvr_vr_native_intents_test PRIVATE pvr_vr_native_intents pvr_iat_hook)
    pvr_enable_warnings(pvr_vr_native_intents_test)
    add_test(NAME vr_native_intents COMMAND pvr_vr_native_intents_test)
    add_executable(pvr_build_catalog_test
        tests/build_catalog/build_catalog_test.cpp
    )
    target_link_libraries(pvr_build_catalog_test PRIVATE pvr_build_catalog)
    pvr_enable_warnings(pvr_build_catalog_test)

    add_test(NAME build_catalog COMMAND pvr_build_catalog_test)

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

    add_executable(pvr_rel32_call_hook_test
        tests/rel32_call_hook/rel32_call_hook_test.cpp
    )
    target_link_libraries(pvr_rel32_call_hook_test PRIVATE pvr_rel32_call_hook)
    pvr_enable_warnings(pvr_rel32_call_hook_test)

    add_test(NAME rel32_call_hook COMMAND pvr_rel32_call_hook_test)

    add_executable(pvr_opengl_eye_targets_test
        tests/opengl_eye_targets/opengl_eye_targets_test.cpp
    )
    set_target_properties(pvr_opengl_eye_targets_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/eye_targets"
    )
    target_link_libraries(
        pvr_opengl_eye_targets_test PRIVATE pvr_opengl_eye_targets pvr_opengl_eye_scissor)
    pvr_enable_warnings(pvr_opengl_eye_targets_test)

    add_test(NAME opengl_eye_targets COMMAND pvr_opengl_eye_targets_test)

    add_executable(pvr_eye_scissor_math_test tests/opengl_eye_targets/eye_scissor_math_test.cpp)
    set_target_properties(pvr_eye_scissor_math_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/eye_targets")
    target_link_libraries(pvr_eye_scissor_math_test PRIVATE pvr_opengl_eye_scissor)
    pvr_enable_warnings(pvr_eye_scissor_math_test)
    add_test(NAME eye_scissor_math COMMAND pvr_eye_scissor_math_test)

    add_executable(pvr_openvr_session_test
        tests/openvr_session/openvr_session_test.cpp
    )
    target_link_libraries(pvr_openvr_session_test PRIVATE pvr_openvr_session)
    pvr_enable_warnings(pvr_openvr_session_test)

    add_test(NAME openvr_session COMMAND pvr_openvr_session_test)

    add_executable(pvr_vr_math_test
        tests/vr_math/vr_math_test.cpp
    )
    target_link_libraries(pvr_vr_math_test PRIVATE pvr_vr_math)
    pvr_enable_warnings(pvr_vr_math_test)

    add_test(NAME vr_math COMMAND pvr_vr_math_test)

    add_executable(pvr_vr_tracking_space_test
        tests/vr_tracking_space/vr_tracking_space_test.cpp
    )
    target_link_libraries(pvr_vr_tracking_space_test PRIVATE pvr_vr_math)
    pvr_enable_warnings(pvr_vr_tracking_space_test)
    add_test(NAME vr_tracking_space COMMAND pvr_vr_tracking_space_test)

    add_executable(pvr_overture_backend_test
        tests/overture_backend/overture_backend_test.cpp
    )
    target_link_libraries(pvr_overture_backend_test PRIVATE pvr_overture_backend)
    pvr_enable_warnings(pvr_overture_backend_test)
    add_test(NAME overture_backend COMMAND pvr_overture_backend_test)

    add_executable(pvr_stereo_render_policy_test
        tests/stereo_render_policy/stereo_render_policy_test.cpp
    )
    target_link_libraries(
        pvr_stereo_render_policy_test PRIVATE pvr_stereo_render_policy)
    pvr_enable_warnings(pvr_stereo_render_policy_test)

    add_test(NAME stereo_render_policy COMMAND pvr_stereo_render_policy_test)

    add_executable(pvr_vr_panel_policy_test
        tests/vr_panel_policy/vr_panel_policy_test.cpp
    )
    target_include_directories(pvr_vr_panel_policy_test PRIVATE src/runtime)
    pvr_enable_warnings(pvr_vr_panel_policy_test)

    add_test(NAME vr_panel_policy COMMAND pvr_vr_panel_policy_test)

    add_executable(pvr_vr_input_state_test
        tests/vr_input_state/vr_input_state_test.cpp
    )
    target_link_libraries(pvr_vr_input_state_test PRIVATE pvr_vr_input_state)
    pvr_enable_warnings(pvr_vr_input_state_test)

    add_test(NAME vr_input_state COMMAND pvr_vr_input_state_test)

    add_executable(pvr_vr_action_input_test tests/vr_input_state/vr_action_input_test.cpp)
    target_link_libraries(pvr_vr_action_input_test PRIVATE pvr_vr_action_input)
    pvr_enable_warnings(pvr_vr_action_input_test)
    add_test(NAME vr_action_input COMMAND pvr_vr_action_input_test)
    add_executable(pvr_vr_hand_pose_test tests/vr_input_state/vr_hand_pose_test.cpp)
    target_link_libraries(pvr_vr_hand_pose_test PRIVATE pvr_vr_math)
    pvr_enable_warnings(pvr_vr_hand_pose_test)
    add_test(NAME vr_hand_pose COMMAND pvr_vr_hand_pose_test)

    add_executable(pvr_vr_settings_test
        tests/vr_settings/vr_settings_test.cpp
    )
    target_link_libraries(pvr_vr_settings_test PRIVATE pvr_vr_settings)
    pvr_enable_warnings(pvr_vr_settings_test)

    add_test(NAME vr_settings COMMAND pvr_vr_settings_test)

    add_executable(pvr_vr_settings_editor_test
        tests/vr_settings_editor/vr_settings_editor_test.cpp
    )
    target_link_libraries(pvr_vr_settings_editor_test PRIVATE pvr_vr_settings)
    pvr_enable_warnings(pvr_vr_settings_editor_test)

    add_test(NAME vr_settings_editor COMMAND pvr_vr_settings_editor_test)

    add_executable(pvr_black_plague_vr_settings_test
        tests/black_plague_settings/black_plague_vr_settings_test.cpp
    )
    target_link_libraries(
        pvr_black_plague_vr_settings_test PRIVATE pvr_black_plague_settings)
    pvr_enable_warnings(pvr_black_plague_vr_settings_test)

    add_test(NAME black_plague_vr_settings COMMAND pvr_black_plague_vr_settings_test)

    add_executable(pvr_vr_settings_store_test
        tests/vr_settings_store/vr_settings_store_test.cpp
    )
    target_link_libraries(
        pvr_vr_settings_store_test PRIVATE pvr_vr_settings_store)
    pvr_enable_warnings(pvr_vr_settings_store_test)

    add_test(NAME vr_settings_store COMMAND pvr_vr_settings_store_test)

    add_executable(pvr_render_target_policy_test
        tests/render_target_policy/render_target_policy_test.cpp
    )
    target_link_libraries(
        pvr_render_target_policy_test PRIVATE pvr_render_target_policy)
    pvr_enable_warnings(pvr_render_target_policy_test)

    add_test(NAME render_target_policy COMMAND pvr_render_target_policy_test)

    add_executable(pvr_pe_large_address_test
        tests/pe_large_address/pe_large_address_test.cpp
    )
    target_link_libraries(
        pvr_pe_large_address_test PRIVATE pvr_pe_large_address)
    pvr_enable_warnings(pvr_pe_large_address_test)

    add_test(NAME pe_large_address COMMAND pvr_pe_large_address_test)

    add_executable(pvr_visual_calibration_test
        tests/visual_calibration/visual_calibration_test.cpp
    )
    target_link_libraries(
        pvr_visual_calibration_test PRIVATE pvr_visual_calibration)
    pvr_enable_warnings(pvr_visual_calibration_test)

    add_test(NAME visual_calibration COMMAND pvr_visual_calibration_test)

    add_executable(pvr_spatial_audio_test
        tests/spatial_audio/spatial_audio_test.cpp
    )
    target_link_libraries(pvr_spatial_audio_test PRIVATE pvr_spatial_audio)
    pvr_enable_warnings(pvr_spatial_audio_test)

    add_test(NAME spatial_audio COMMAND pvr_spatial_audio_test)

    add_executable(pvr_camera_matrix_override_test
        tests/camera_matrix_override/camera_matrix_override_test.cpp
    )
    target_include_directories(
        pvr_camera_matrix_override_test PRIVATE src/backends/black_plague)
    target_link_libraries(
        pvr_camera_matrix_override_test PRIVATE pvr_hpl1_camera_adapter)
    pvr_enable_warnings(pvr_camera_matrix_override_test)

    add_test(NAME camera_matrix_override COMMAND pvr_camera_matrix_override_test)
endif()
