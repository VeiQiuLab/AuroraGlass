include(CMakePackageConfigHelpers)

foreach(_t
    AuroraGlassMaterial
    AuroraGlassControls
    AuroraGlassMotion
    AuroraGlassMotionControls
)
    set_property(
        TARGET ${_t}
        PROPERTY INTERFACE_INCLUDE_DIRECTORIES
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>;$<INSTALL_INTERFACE:include>"
    )
endforeach()

set_property(
    TARGET AuroraGlassCore
    PROPERTY INTERFACE_INCLUDE_DIRECTORIES
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>;$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>;$<INSTALL_INTERFACE:include>"
)

set_property(
    TARGET AuroraGlassWin32Adapter
    PROPERTY INTERFACE_INCLUDE_DIRECTORIES
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/adapters>;$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>;$<INSTALL_INTERFACE:include>"
)

install(
    TARGETS
        AuroraGlassMaterial
        AuroraGlassCore
        AuroraGlassControls
        AuroraGlassMotion
        AuroraGlassMotionControls
        AuroraGlassWin32Adapter
    EXPORT AuroraGlassTargets
    ARCHIVE DESTINATION "lib/$<CONFIG>"
)

install(
    TARGETS AuroraGlassWpfInterop AuroraGlassWinUIInterop
    RUNTIME DESTINATION "bin/$<CONFIG>"
    ARCHIVE DESTINATION "lib/$<CONFIG>"
)

file(
    STRINGS
    "${CMAKE_SOURCE_DIR}/api/native_public_headers.txt"
    P8_PUBLIC_HEADERS
)

foreach(_h IN LISTS P8_PUBLIC_HEADERS)
    if(_h STREQUAL "")
        continue()
    endif()

    if(_h MATCHES "^src/(.+)$")
        set(_r "${CMAKE_MATCH_1}")
    elseif(_h MATCHES "^adapters/(.+)$")
        set(_r "${CMAKE_MATCH_1}")
    else()
        set(_r "${_h}")
    endif()

    get_filename_component(_d "${_r}" DIRECTORY)

    install(
        FILES "${CMAKE_SOURCE_DIR}/${_h}"
        DESTINATION "include/${_d}"
    )
endforeach()

install(
    FILES "${CMAKE_BINARY_DIR}/generated/auroraglass/version.h"
    DESTINATION "include/auroraglass"
)

install(FILES "${CMAKE_SOURCE_DIR}/VERSION" DESTINATION ".")

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/AuroraGlassConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/AuroraGlassConfig.cmake"
    INSTALL_DESTINATION "lib/cmake/AuroraGlass"
)

write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/AuroraGlassConfigVersion.cmake"
    VERSION "${AURORAGLASS_SDK_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(
    EXPORT AuroraGlassTargets
    FILE AuroraGlassTargets.cmake
    NAMESPACE AuroraGlass::
    DESTINATION "lib/cmake/AuroraGlass"
)

install(
    FILES
        "${CMAKE_BINARY_DIR}/AuroraGlassConfig.cmake"
        "${CMAKE_BINARY_DIR}/AuroraGlassConfigVersion.cmake"
    DESTINATION "lib/cmake/AuroraGlass"
)
# P8 runtime shaders
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/shaders/"
    DESTINATION "shaders"
    FILES_MATCHING PATTERN "*.hlsl"
)
# P8 WPF managed assembly
install(
    FILES "${CMAKE_BINARY_DIR}/managed/AuroraGlass.Wpf/bin/x64/$<CONFIG>/net10.0-windows/AuroraGlass.Wpf.dll"
    DESTINATION "managed/WPF/$<CONFIG>"
)

# P8 WinUI managed assembly
install(
    FILES "${CMAKE_BINARY_DIR}/managed/AuroraGlass.WinUI/bin/x64/$<CONFIG>/net10.0-windows10.0.19041.0/win-x64/AuroraGlass.WinUI.dll"
    DESTINATION "managed/WinUI/$<CONFIG>"
)
