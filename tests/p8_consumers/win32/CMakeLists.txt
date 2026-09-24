cmake_minimum_required(VERSION 3.20)
project(P8FreshWin32 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(AuroraGlass CONFIG REQUIRED)

add_executable(P8FreshWin32 WIN32 main.cpp)

target_link_libraries(P8FreshWin32 PRIVATE
    AuroraGlass::AuroraGlassCore
    AuroraGlass::AuroraGlassWin32Adapter
    d3d11 dxgi d3dcompiler comctl32
)