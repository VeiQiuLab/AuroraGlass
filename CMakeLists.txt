cmake_minimum_required(VERSION 3.20)
project(AuroraGlass LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8 /EHsc /permissive-)
endif()

set(CORE_SOURCES
    src/core/d3d11_device.cpp
    src/core/glass_params.cpp
    src/core/shader_library.cpp
    src/core/background_source.cpp
    src/core/glass_renderer.cpp
    # P1 additions (parallel with P0 files; P0 files retained until migration completes)
    src/core/glass_material.cpp
    src/core/glass_surface.cpp
)

add_library(AuroraGlassCore STATIC ${CORE_SOURCES})
target_include_directories(AuroraGlassCore PUBLIC src)

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
