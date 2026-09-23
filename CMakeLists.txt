cmake_minimum_required(VERSION 3.20)
project(AuroraGlass LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8 /EHsc /permissive-)
endif()

# Pure-CPU material value type (GlassMaterial) is factored into its own small
# library so that CPU-only consumers (Controls) can link it without pulling in
# any D3D/GPU objects. No source or API change — build organization only.
add_library(AuroraGlassMaterial STATIC src/core/glass_material.cpp)
target_include_directories(AuroraGlassMaterial PUBLIC src)

set(CORE_SOURCES
    src/core/d3d11_device.cpp
    src/core/glass_params.cpp
    src/core/shader_library.cpp
    src/core/background_source.cpp
    src/core/glass_renderer.cpp
    # P1 additions (parallel with P0 files; P0 files retained until migration completes)
    src/core/glass_surface.cpp
)

add_library(AuroraGlassCore STATIC ${CORE_SOURCES})
target_include_directories(AuroraGlassCore PUBLIC src)
target_link_libraries(AuroraGlassCore PUBLIC AuroraGlassMaterial)

add_executable(P0_Proof samples/p0_proof/main.cpp)
target_link_libraries(P0_Proof PRIVATE AuroraGlassCore d3d11 dxgi d3dcompiler)

# P1 independent runtime smoke test (does NOT modify the frozen P0 sample).
add_executable(P1_Smoke samples/p1_smoke/main.cpp)
target_link_libraries(P1_Smoke PRIVATE AuroraGlassCore d3d11 dxgi d3dcompiler)
add_custom_command(TARGET P1_Smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P1_Smoke>/shaders)

# P1 visual parity sample — sits alongside FROZEN p0_proof and p1_smoke.
# p0_proof = frozen P0 golden baseline (DO NOT MODIFY)
# p1_smoke = runtime/API verification
# p1_proof = P1 public API visual parity
add_executable(P1_Proof samples/p1_proof/main.cpp)
target_link_libraries(P1_Proof PRIVATE AuroraGlassCore d3d11 dxgi d3dcompiler)
add_custom_command(TARGET P1_Proof POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P1_Proof>/shaders)

# Copy HLSL sources next to the executable so the runtime compiler finds them.
add_custom_command(TARGET P0_Proof POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P0_Proof>/shaders)

# ------------------------------------------------------------
# P1 automated tests (minimal, no external framework).
# ------------------------------------------------------------
enable_testing()

add_executable(P1_Tests tests/p1_tests.cpp)
target_link_libraries(P1_Tests PRIVATE AuroraGlassCore d3d11 dxgi d3dcompiler)
add_custom_command(TARGET P1_Tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P1_Tests>/shaders)
add_test(NAME P1_Tests COMMAND P1_Tests)

# ------------------------------------------------------------
# P2 Slice 1 — repeated lifecycle stress (tests/ only; no API change).
# ------------------------------------------------------------
add_executable(P2_Stress_Tests tests/p2_stress_tests.cpp)
target_link_libraries(P2_Stress_Tests PRIVATE AuroraGlassCore d3d11 dxgi d3dcompiler psapi)
add_custom_command(TARGET P2_Stress_Tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P2_Stress_Tests>/shaders)
add_test(NAME P2_Stress_Tests COMMAND P2_Stress_Tests)

# ------------------------------------------------------------
# P2 Slice 2 — resize / zero-size robustness (tests/ only).
# ------------------------------------------------------------
add_executable(P2_Resize_Tests tests/p2_resize_tests.cpp)
target_link_libraries(P2_Resize_Tests PRIVATE AuroraGlassCore d3d11 dxgi d3dcompiler)
add_custom_command(TARGET P2_Resize_Tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P2_Resize_Tests>/shaders)
add_test(NAME P2_Resize_Tests COMMAND P2_Resize_Tests)

# ------------------------------------------------------------
# P2 Slice 5 — error-path coverage (tests/ only).
# ------------------------------------------------------------
add_executable(P2_ErrorPath_Tests tests/p2_errorpath_tests.cpp)
target_link_libraries(P2_ErrorPath_Tests PRIVATE AuroraGlassCore d3d11 dxgi d3dcompiler)
add_custom_command(TARGET P2_ErrorPath_Tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P2_ErrorPath_Tests>/shaders)
add_test(NAME P2_ErrorPath_Tests COMMAND P2_ErrorPath_Tests)

# ------------------------------------------------------------
# P2 diagnostic tool — DebugView-style OutputDebugString capture
# (used to inspect ReportLiveDeviceObjects output; not a sample).
# ------------------------------------------------------------
add_executable(P2_LiveObject_Capture tests/liveobject_capture.cpp)

# ------------------------------------------------------------
# P2 Slice 2 — resize/minimize/restore diagnostic host (new sample;
# does not modify frozen p0_proof / p1_smoke / p1_proof / p1_tests).
# ------------------------------------------------------------
add_executable(P2_Resize_Smoke samples/p2_resize_smoke/main.cpp)
target_link_libraries(P2_Resize_Smoke PRIVATE AuroraGlassCore d3d11 dxgi d3dcompiler)
add_custom_command(TARGET P2_Resize_Smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P2_Resize_Smoke>/shaders)

# ------------------------------------------------------------
# P2 Slice 3 — DPI / multi-monitor validation host (host-only; DPI never
# enters Core). New sample; frozen baselines untouched.
# ------------------------------------------------------------
add_executable(P2_Dpi_Smoke samples/p2_dpi_smoke/main.cpp)
target_link_libraries(P2_Dpi_Smoke PRIVATE AuroraGlassCore d3d11 dxgi d3dcompiler)
add_custom_command(TARGET P2_Dpi_Smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P2_Dpi_Smoke>/shaders)

# ------------------------------------------------------------
# P2 Slice 4 — Core rendering performance baseline (offscreen).
# New sample; frozen baselines untouched.
# ------------------------------------------------------------
add_executable(P2_Bench samples/p2_bench/main.cpp)
target_link_libraries(P2_Bench PRIVATE AuroraGlassCore d3d11 dxgi d3dcompiler)
add_custom_command(TARGET P2_Bench POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P2_Bench>/shaders)

# ------------------------------------------------------------
# P3 Slice A — Controls geometry / state / input contract.
# PURE CPU: no D3D, no dxgi, no d3dcompiler, no HWND, no Core dependency.
# Deliberately a separate library so a GPU dependency cannot creep in.
# ------------------------------------------------------------
set(CONTROLS_SOURCES
    src/controls/control_geometry.cpp
    src/controls/control_interaction.cpp
)
add_library(AuroraGlassControls STATIC ${CONTROLS_SOURCES})
target_include_directories(AuroraGlassControls PUBLIC src)
# Controls depend on the GlassMaterial value type only (pure CPU) — never on
# GlassSurface / D3D.
target_link_libraries(AuroraGlassControls PUBLIC AuroraGlassMaterial)

add_executable(P3_Controls_Tests tests/p3_controls_tests.cpp)
target_link_libraries(P3_Controls_Tests PRIVATE AuroraGlassControls)
add_test(NAME P3_Controls_Tests COMMAND P3_Controls_Tests)

# P3 Slice B — state -> GlassMaterial mapping. Pure CPU; depends on the frozen
# Core value type GlassMaterial (header-only) but NOT on GlassSurface/D3D.
add_executable(P3_Style_Tests tests/p3_style_tests.cpp)
target_link_libraries(P3_Style_Tests PRIVATE AuroraGlassControls)
add_test(NAME P3_Style_Tests COMMAND P3_Style_Tests)

# P3 Slice C — multi-control batch path (PrepareFrame / RenderRect).
add_executable(P3_Batch_Tests tests/p3_batch_tests.cpp)
target_link_libraries(P3_Batch_Tests PRIVATE AuroraGlassCore d3d11 dxgi d3dcompiler)
add_custom_command(TARGET P3_Batch_Tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P3_Batch_Tests>/shaders)
add_test(NAME P3_Batch_Tests COMMAND P3_Batch_Tests)

# P3 Slice D - interactive control semantics (pure CPU; links Controls only).
add_executable(P3_Interactive_Tests tests/p3_interactive_tests.cpp)
target_link_libraries(P3_Interactive_Tests PRIVATE AuroraGlassControls)
add_test(NAME P3_Interactive_Tests COMMAND P3_Interactive_Tests)

# P3 Slice D - sample-only drag helper tests (pure CPU).
add_executable(P3_Drag_Helper_Tests tests/p3_drag_helper_tests.cpp)
target_include_directories(P3_Drag_Helper_Tests PRIVATE samples/p3_interactive_smoke)
target_link_libraries(P3_Drag_Helper_Tests PRIVATE AuroraGlassControls)
add_test(NAME P3_Drag_Helper_Tests COMMAND P3_Drag_Helper_Tests)

# P3 Slice D - interactive controls sample (new; frozen baselines untouched).
add_executable(P3_Interactive_Smoke samples/p3_interactive_smoke/main.cpp)
target_link_libraries(P3_Interactive_Smoke PRIVATE AuroraGlassCore AuroraGlassControls d3d11 dxgi d3dcompiler windowscodecs ole32)
add_custom_command(TARGET P3_Interactive_Smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P3_Interactive_Smoke>/shaders)
add_custom_command(TARGET P3_Interactive_Smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/samples/p3_interactive_smoke/shaders
        $<TARGET_FILE_DIR:P3_Interactive_Smoke>/shaders)

# P3 Slice C — multi-control visual sample (new; frozen baselines untouched).
add_executable(P3_Controls_Smoke samples/p3_controls_smoke/main.cpp)
target_link_libraries(P3_Controls_Smoke PRIVATE AuroraGlassCore d3d11 dxgi d3dcompiler)
# Core glass shaders (shared).
add_custom_command(TARGET P3_Controls_Smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P3_Controls_Smoke>/shaders)
# Sample-only host blit shader (NOT part of the AuroraGlass Core glass pipeline).
add_custom_command(TARGET P3_Controls_Smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/samples/p3_controls_smoke/shaders
        $<TARGET_FILE_DIR:P3_Controls_Smoke>/shaders)

# P3 Slice E - independent Win32 consumer reusability proof.
add_executable(P3_Reuse_Smoke samples/p3_reuse_smoke/main.cpp)
target_link_libraries(P3_Reuse_Smoke PRIVATE AuroraGlassCore AuroraGlassControls d3d11 dxgi d3dcompiler gdi32)
add_custom_command(TARGET P3_Reuse_Smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P3_Reuse_Smoke>/shaders)

# P4 Slice A - optional CPU-only motion primitives.
# No dependency on Core, Controls, D3D, HWND, or host-framework services.
add_library(AuroraGlassMotion STATIC
    src/motion/motion.cpp
)
target_include_directories(AuroraGlassMotion PUBLIC src)
target_compile_features(AuroraGlassMotion PUBLIC cxx_std_20)

add_executable(P4_Motion_Tests tests/p4_motion_tests.cpp)
target_link_libraries(P4_Motion_Tests PRIVATE AuroraGlassMotion)
add_test(NAME P4_Motion_Tests COMMAND P4_Motion_Tests)

# P4 Slice B - semantic controls -> animated presentation bindings.
# One-way dependency: P3 semantic state -> P4 presentation motion.
add_library(AuroraGlassMotionControls STATIC
    src/motion/control_motion.cpp
)
target_include_directories(AuroraGlassMotionControls PUBLIC src)
target_compile_features(AuroraGlassMotionControls PUBLIC cxx_std_20)
target_link_libraries(AuroraGlassMotionControls
    PUBLIC
        AuroraGlassMotion
        AuroraGlassControls
)

add_executable(P4_Motion_Integration_Tests
    tests/p4_motion_integration_tests.cpp
)
target_link_libraries(P4_Motion_Integration_Tests
    PRIVATE
        AuroraGlassMotionControls
)
add_test(
    NAME P4_Motion_Integration_Tests
    COMMAND P4_Motion_Integration_Tests
)

# P4 Slice B - runtime motion integration smoke.
add_executable(P4_Motion_Smoke
    samples/p4_motion_smoke/main.cpp
)
target_link_libraries(P4_Motion_Smoke
    PRIVATE
        AuroraGlassCore
        AuroraGlassControls
        AuroraGlassMotionControls
        d3d11
        dxgi
        d3dcompiler
)
add_custom_command(TARGET P4_Motion_Smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:P4_Motion_Smoke>/shaders
)

# P4 continuation - deterministic highlight/light-follow coverage.
add_executable(P4_Light_Follow_Tests
    tests/p4_light_follow_tests.cpp
)
target_link_libraries(P4_Light_Follow_Tests
    PRIVATE
        AuroraGlassMotionControls
)
add_test(
    NAME P4_Light_Follow_Tests
    COMMAND P4_Light_Follow_Tests
)

# P4 reduced-motion focused deterministic coverage.
add_executable(P4_Reduced_Motion_Tests
    tests/p4_reduced_motion_tests.cpp
)

target_link_libraries(P4_Reduced_Motion_Tests PRIVATE
    AuroraGlassMotionControls
    AuroraGlassControls
)

add_test(
    NAME P4_Reduced_Motion_Tests
    COMMAND P4_Reduced_Motion_Tests
)

# P5 Win32 Adapter - Slice A: host HWND attachment lifecycle.
add_library(AuroraGlassWin32Adapter STATIC
    adapters/win32/win32_host_attachment.cpp
)

target_include_directories(AuroraGlassWin32Adapter PUBLIC
    adapters
)

target_compile_features(AuroraGlassWin32Adapter PUBLIC
    cxx_std_20
)

target_link_libraries(AuroraGlassWin32Adapter PRIVATE
    Comctl32
)

add_executable(P5_Win32_Attachment_Tests
    tests/p5_win32_attachment_tests.cpp
)

target_link_libraries(P5_Win32_Attachment_Tests PRIVATE
    AuroraGlassWin32Adapter
)

add_test(
    NAME P5_Win32_Attachment_Tests
    COMMAND P5_Win32_Attachment_Tests
)

add_executable(P5_Win32_Host_Metrics_Tests
    tests/p5_win32_host_metrics_tests.cpp
)

target_link_libraries(P5_Win32_Host_Metrics_Tests PRIVATE
    AuroraGlassWin32Adapter
)

add_test(
    NAME P5_Win32_Host_Metrics_Tests
    COMMAND P5_Win32_Host_Metrics_Tests
)

# P5 Slice C - minimal Win32 -> frozen P3 glass-control input bridge.
target_sources(AuroraGlassWin32Adapter PRIVATE
    adapters/win32/win32_control_input_bridge.cpp
)

target_link_libraries(AuroraGlassWin32Adapter PUBLIC
    AuroraGlassControls
)

add_executable(P5_Win32_Control_Input_Bridge_Tests
    tests/p5_win32_control_input_bridge_tests.cpp
)

target_link_libraries(P5_Win32_Control_Input_Bridge_Tests PRIVATE
    AuroraGlassWin32Adapter
)

add_test(
    NAME P5_Win32_Control_Input_Bridge_Tests
    COMMAND P5_Win32_Control_Input_Bridge_Tests
)
