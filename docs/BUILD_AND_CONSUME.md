# Build and Consume AuroraGlass

AuroraGlass is consumed as a staged Windows SDK. The canonical SDK version is read from the repository root VERSION file.

## Build

Configure and build from the repository root:

    cmake -S . -B build
    cmake --build build --config Debug

The canonical repository build is serial.

## Stage the SDK

Install into a standalone prefix:

    cmake --install build --config Debug --prefix build/sdk-stage/AuroraGlass-0.9.0

The staged SDK contains:

    VERSION
    include/
    lib/
    bin/
    managed/
    shaders/

The native public include tree is derived from api/native_public_headers.txt plus the generated AuroraGlass version header.

Internal renderer and device headers are not part of the SDK.

## Native Win32 consumption

Point an external CMake project at the installed prefix:

    cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/path/to/AuroraGlass-0.9.0

Then consume AuroraGlass through:

    find_package(AuroraGlass CONFIG REQUIRED)

Exported native targets include:

    AuroraGlass::AuroraGlassMaterial
    AuroraGlass::AuroraGlassCore
    AuroraGlass::AuroraGlassControls
    AuroraGlass::AuroraGlassMotion
    AuroraGlass::AuroraGlassMotionControls
    AuroraGlass::AuroraGlassWin32Adapter

The public Win32 include root is include/win32. For example:

    #include "win32/win32_host_attachment.h"

A consumer does not need the AuroraGlass source tree, tests, samples, or internal headers.

## Runtime resources

AuroraGlass production rendering loads HLSL resources at runtime.

Deploy the staged shaders directory with the consuming executable. The staged directory contains the production shader files used by GlassSurface and the managed render hosts.

## WPF consumption

Managed assembly:

    managed/WPF/Debug/AuroraGlass.Wpf.dll

Native bridge:

    bin/Debug/AuroraGlassWpfInterop.dll

Reference the staged managed assembly from the WPF application and deploy the native bridge and shaders with the application output.

No ProjectReference to the AuroraGlass repository is required.

The P8 fresh WPF consumer validates:

    WpfHostAttachment
    WpfRenderHost
    WpfGlassMaterial
    actual rendered frames
    clean detach and teardown

## WinUI 3 consumption

Managed assembly:

    managed/WinUI/Debug/AuroraGlass.WinUI.dll

Native bridge:

    bin/Debug/AuroraGlassWinUIInterop.dll

The current validated consumer uses Microsoft.WindowsAppSDK 2.5.1.

Reference the staged managed assembly from the WinUI application and deploy the native bridge and shaders with the application output.

No ProjectReference to the AuroraGlass repository is required.

The P8 fresh WinUI consumer validates:

    WinUIHostAttachment
    WinUIControlInputBridge
    WinUIRenderHost
    WinUIGlassMaterial
    actual rendered frames
    clean detach and teardown

## Version

Current SDK version:

    0.9.0

VERSION is the canonical source. The same version is propagated to the generated native version API, CMake package metadata, staged VERSION file, and managed AuroraGlass assemblies.

## P8 Slice B validation

The SDK package and external-consumption boundary is covered by:

    P8_SDK_Install_Layout
    P8_Fresh_Win32_Consumer
    P8_Fresh_WPF_Consumer
    P8_Fresh_WinUI_Consumer

These tests create or use fresh consumers outside the AuroraGlass source tree and consume staged SDK artifacts rather than ProjectReference or internal source paths.