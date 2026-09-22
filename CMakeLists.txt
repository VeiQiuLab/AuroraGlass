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
