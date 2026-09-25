# AuroraGlass Troubleshooting

This guide covers real problems observed while building and consuming AuroraGlass
as a staged Windows SDK. It reflects the current production behavior and the
validated environment (see docs/COMPATIBILITY.md).

Status vocabulary used here is the same as the compatibility matrix:
VALIDATED / SUPPORTED BY DESIGN BUT NOT VALIDATED / NOT TESTED / NOT SUPPORTED.

The canonical SDK version is read from the repository root VERSION file
(currently 0.8.1).

---

## 1. SDK / CMake

### find_package(AuroraGlass CONFIG REQUIRED) fails

- Symptom: configure stops with a missing package configuration file error.
- Likely cause: AuroraGlassConfig.cmake is not on the search path, or the
  staged prefix was never produced.
- How to verify: confirm lib/cmake/AuroraGlass/AuroraGlassConfig.cmake exists
  under the staged prefix, and that the prefix you passed is the stage root
  (build/sdk-stage/AuroraGlass-0.8.1), not its lib or include subdir.
- Fix: stage the SDK (cmake --install build --config Debug --prefix <stage>)
  and point CMake at the stage root with -DCMAKE_PREFIX_PATH=<stage>.

### AuroraGlassConfig.cmake not found / wrong CMAKE_PREFIX_PATH

- Symptom: the config file exists on disk but CMake does not use it.
- Likely cause: CMAKE_PREFIX_PATH points at a subdirectory, or uses
  inconsistent forward/back slashes.
- How to verify: print the search path and check the exact stage root.
- Fix: pass the stage root itself; CMake appends lib/cmake/AuroraGlass
  automatically for CONFIG packages.

### Debug / Release artifact mismatch

- Symptom: link succeeds but the program fails to start, or a Debug build links
  a Release import library.
- Likely cause: configuring a consumer as Release while passing a Debug
  prefix, or vice versa.
- How to verify: the staged bin/<Config> and lib/<Config> folders are
  config-specific; match the consumer config to the staged config.
- Fix: build the consumer with the same config that was staged. P8 validates
  Debug. Release is NOT TESTED as an SDK compatibility claim (a Release
  benchmark build exists only for measurement).

### Missing import library

- Symptom: unresolved external symbols when linking a native consumer.
- Likely cause: the target was not linked, or the wrong config lib folder was
  on the link path.
- How to verify: the staged lib/<Config>/ contains AuroraGlassCore.lib,
  AuroraGlassMaterial.lib, AuroraGlassControls.lib, AuroraGlassMotion.lib,
  AuroraGlassMotionControls.lib, AuroraGlassWin32Adapter.lib.
- Fix: link the exported CMake targets (for example
  AuroraGlass::AuroraGlassCore).

### Missing runtime DLL

- Symptom: the program builds and links, but fails at startup with a missing
  DLL error.
- Likely cause: the native interop DLLs were not deployed next to the
  executable.
- How to verify: bin/<Config>/AuroraGlassWpfInterop.dll and
  bin/<Config>/AuroraGlassWinUIInterop.dll must be copied to the consumer
  output directory (or be on PATH).
- Fix: deploy the native bridge DLLs with the application output.

---

## 2. Public Headers

### Consumer accidentally includes an internal header

- Symptom: a consumer includes core/d3d11_device.h or core/glass_renderer.h
  and fails to compile.
- Likely cause: reaching into implementation headers that are NOT part of the
  SDK.
- How to verify: the installed include tree contains only the headers listed in
  api/native_public_headers.txt plus the generated auroraglass/version.h.
  Internal headers such as core/d3d11_device.h, core/glass_renderer.h, and
  core/shader_library.h are NOT installed.
- Fix: include only the public headers. The public contract is the installed
  include tree and api/native_public_headers.txt.

### Header not found / wrong include root

- Symptom: a public header such as win32/win32_host_attachment.h is not found.
- Likely cause: the include root is wrong.
- How to verify: the public Win32 include root is include/win32; the core
  public root is include/.
- Fix: add the staged include directory to the include path and include the
  public path, for example win32/win32_host_attachment.h.

Do NOT tell consumers to include src/* or any internal implementation header.
The SDK intentionally does not ship them.

---

## 3. Shader / Runtime Resources

### Shader files missing

- Symptom: GlassSurface::Create returns Status::ShaderError, or a render host
  reports a non-zero Core status.
- Likely cause: the production HLSL files are loaded from disk at runtime and
  were not deployed with the executable.
- How to verify: Core discovers shaders by searching, in order, for glass.hlsl:
    1. <exeDir>/shaders
    2. <exeDir>/../shaders
    3. <exeDir>/../../shaders
    4. <current working dir>/shaders
    5. <current working dir>/../shaders
  The first candidate that contains glass.hlsl wins.
- Fix: deploy the staged shaders/ directory such that one of the candidates
  above resolves. Deploying shaders/ next to the executable is the simplest.

### Render host initializes but glass output missing

- Symptom: the host and device initialize and the frame count advances, but no
  glass is visible.
- Likely cause: the shaders resolved to an unexpected directory (stale or
  empty), or the background resource was not supplied.
- How to verify: confirm the resolved shaders/ directory contains glass.hlsl,
  blur.hlsl, background.hlsl, and fullscreen_triangle.hlsl.
- Fix: remove stray shaders/ directories so the intended one is found first.

### Runtime working directory / resource path mismatch

- Symptom: running from the build tree works, but running the deployed app from
  another working directory fails to find shaders.
- Likely cause: the app relied on the current-working-directory candidate
  rather than the executable-relative candidates.
- How to verify: launch the app from an unrelated working directory.
- Fix: deploy shaders/ next to the executable so discovery does not depend on
  the working directory.

---

## 4. Native Win32

### Attach returns false

- Symptom: Win32HostAttachment::Attach(hwnd) returns false.
- Likely cause: the HWND is null/not a valid top-level window, or a different
  HWND is already attached.
- How to verify: IsAttached() / Window() reflect current state.
- Fix: pass a live HWND; re-attaching the SAME HWND is idempotent, but
  attaching a DIFFERENT HWND while attached fails. Call Detach() first.

### Invalid HWND

- Symptom: attach fails or metrics are never delivered.
- Likely cause: attaching before the window handle exists.
- How to verify: ensure the HWND is created and valid before Attach.
- Fix: attach after the window is created (for WPF, see the SourceInitialized
  note in section 5).

### Different HWND already attached

- Symptom: a second attach fails.
- Likely cause: the adapter supports one HWND at a time.
- Fix: Detach() the previous window before attaching another.

### 0x0 / minimized behavior

- Symptom: client width/height read 0.
- Likely cause: the window is minimized. This is a VALID state, not an error.
- How to verify: Win32HostMetrics reports {0, 0, dpi} while minimized.
- Fix: skip rendering while the client size is zero; resume when the window is
  restored. Do not treat a zero client size as a failure.

### Resize handling

- Symptom: glass output does not follow the window size.
- Likely cause: the host did not react to the resize callback or did not
  resize its render target.
- How to verify: the adapter delivers a resize notification with updated
  Win32HostMetrics.
- Fix: on resize, resize the swap chain/render target and the GlassSurface.

### Input / capture cleanup

- Symptom: mouse capture is not released after a control interaction.
- Likely cause: the input bridge was not detached, or a drag was interrupted.
- How to verify: the input bridge exposes whether it owns capture; a clean
  detach releases it.
- Fix: always Detach() the input bridge and host attachment during teardown;
  the samples verify a clean teardown.

---

## 5. WPF

### AuroraGlassWpfInterop.dll not found

- Symptom: the managed assembly loads, but a native call fails.
- Likely cause: AuroraGlassWpfInterop.dll was not deployed next to the
  application.
- How to verify: the staged bin/<Config>/AuroraGlassWpfInterop.dll exists.
- Fix: deploy the native bridge DLL with the WPF application output.

### Managed assembly loads but native interop fails

- Symptom: WpfHostAttachment / WpfRenderHost calls throw or return failure.
- Likely cause: the native bridge is missing, is the wrong config, or has a
  different bitness.
- How to verify: confirm the bridge is present, x64, and matches the staged
  config.
- Fix: deploy the matching x64 bridge.

### Attach before SourceInitialized

- Symptom: WpfHostAttachment.Attach(window) returns false.
- Likely cause: the underlying HWND does not exist yet. WPF creates it at
  SourceInitialized.
- How to verify: the sample attaches only after SourceInitialized.
- Fix: subscribe to SourceInitialized and attach there, not earlier.

### DIP -> physical conversion mistakes

- Symptom: hit-testing or bounds are offset at non-100% DPI.
- Likely cause: using WPF DIP coordinates directly instead of converting.
- How to verify: WpfHostAttachment.DipToPhysical converts using the native
  window DPI.
- Fix: convert DIP to physical before passing coordinates to the native input
  bridge. Use DipToPhysical, not an assumed 96 DPI.

### 0x0 minimized state

- Symptom: metrics report zero size.
- Likely cause: the window is minimized (a valid state).
- Fix: treat zero-size as legal and pause rendering until restored.

### Render host teardown

- Symptom: leaked resources or a non-zero Core status after close.
- Likely cause: the render host was not disposed / detached cleanly.
- How to verify: the P6 formal sample asserts a clean detach and teardown.
- Fix: dispose the render host and detach the host attachment and input bridge.

### Motion

The WPF Motion managed boundary is NOT EXPOSED. Motion is a CPU-only native
layer and is not currently surfaced as a managed WPF API. This is a design
boundary, NOT a fault.

---

## 6. WinUI 3

### Windows App SDK / runtime mismatch

- Symptom: the WinUI app fails to start or cannot find the Windows App SDK
  runtime.
- Likely cause: the deployed Windows App SDK runtime does not match the
  referenced package.
- How to verify: the validated path uses Microsoft.WindowsAppSDK 2.5.1.
- Fix: align the runtime with the referenced Windows App SDK version.

### AuroraGlassWinUIInterop.dll not found

- Symptom: the native bridge cannot be located.
- Likely cause: AuroraGlassWinUIInterop.dll was not deployed.
- How to verify: the staged bin/<Config>/AuroraGlassWinUIInterop.dll exists.
- Fix: deploy the native bridge DLL with the application output.

### Real HWND acquisition failure

- Symptom: the host attachment cannot obtain an HWND.
- Likely cause: obtaining the window handle before the window exists.
- How to verify: the sample acquires the HWND via the window-native handle
  after the window is created.
- Fix: acquire the HWND after the window is created, then attach.

### LogicalToPhysical mistakes

- Symptom: input or bounds are offset at non-100% scale.
- Likely cause: using logical units directly.
- How to verify: WinUIHostAttachment.LogicalToPhysical converts using the
  current metrics scale.
- Fix: convert logical coordinates to physical before native input calls. The
  authoritative DPI is the native HWND DPI (see section 7).

### Render child lifecycle / input cleanup

- Symptom: resources leak, or capture is retained.
- Likely cause: the render child or input bridge was not torn down.
- How to verify: the P7 formal sample asserts clean teardown.
- Fix: dispose the render child and detach the input bridge and host
  attachment.

The validated WinUI configuration uses WindowsPackageType=None (unpackaged)
and win-x64.

### Motion

The WinUI Motion managed boundary is NOT EXPOSED. This is a design boundary,
not a fault.

---

## 7. DPI

- Authoritative DPI is the native HWND DPI. The adapter reads it via
  GetDpiForWindow and reports it in the host metrics.
- WPF and WinUI managed scale values (DIP / logical) are conversion helpers
  only. They are not the source of truth.
- On WM_DPICHANGED, the repository host contract uses the X DPI from
  LOWORD(wParam) as the single window DPI value and does NOT apply the
  suggested RECT.
- Real cross-monitor DPI transition is NOT TESTED. No multi-monitor transition
  proof is claimed. Do not read the DPI handling above as a guarantee that
  live cross-monitor transitions are fully supported.

---

## 8. Known Non-Blocking Warnings

These are warnings, not AuroraGlass runtime failures. They do not indicate a
defect in the SDK.

- MSB3539 (BaseIntermediateOutputPath was modified after it was used by
  MSBuild): emitted while building the managed samples/adapters because they set
  a custom intermediate output path. Builds still succeed. It is a build-system
  advisory, not a runtime problem.
- PowerShell 5.1 UTF-8 display mojibake: non-ASCII output (for example
  localized MSBuild text) can appear garbled in a PowerShell 5.1 console. This
  is a console encoding display issue, not a build or runtime failure.
- Git LF -> CRLF warning: a line-ending advisory from Git on Windows. It does
  not affect compiled output.

A warning is NOT the same as an AuroraGlass runtime failure. Do not hide a real
build error behind these; if a build fails, treat the error, not the warning.

---

## 9. Diagnosis Checklist

Work top to bottom:

- [ ] VERSION / SDK layout correct (staged VERSION matches the canonical VERSION)
- [ ] Native bridge DLL present next to the executable
- [ ] shader / runtime resources present (shaders/glass.hlsl reachable)
- [ ] correct config (Debug/Release matches the staged config; P8 validates Debug)
- [ ] valid HWND (attached after creation / SourceInitialized)
- [ ] non-zero render size (0x0 only while minimized)
- [ ] DPI conversion correct (use native HWND DPI)
- [ ] no internal header dependency (only api/native_public_headers.txt + auroraglass/version.h)
- [ ] sample / external consumer reference works
