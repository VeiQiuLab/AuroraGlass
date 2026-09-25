#pragma once

// ============================================================
// AuroraGlass P6 WPF Adapter - airspace-safe composition bridge.
//
// Renders AuroraGlass (D3D11/HLSL) into an OFFSCREEN shared BGRA
// surface, then exposes a D3D9Ex level-0 IDirect3DSurface9* that a
// managed WPF D3DImage can consume. No HwndHost, no WS_CHILD window,
// no native child HWND over the WPF visual tree.
//
// This is an additive path alongside the existing HwndHost-based
// AuroraGlassWpfRenderHost; it does not replace or break it.
// ============================================================

#include "wpf/wpf_material_bridge.h"
#include "wpf/wpf_render_host_bridge.h"

#include <stdint.h>

#if defined(_WIN32)
    #if defined(AURORAGLASS_WPF_INTEROP_EXPORTS)
        #define AURORAGLASS_WPF_COMP_API __declspec(dllexport)
    #else
        #define AURORAGLASS_WPF_COMP_API __declspec(dllimport)
    #endif
#else
    #define AURORAGLASS_WPF_COMP_API
#endif

#if defined(__cplusplus)
    #define AURORAGLASS_WPF_COMP_NOEXCEPT noexcept
    extern "C" {
#else
    #define AURORAGLASS_WPF_COMP_NOEXCEPT
#endif

typedef struct AuroraGlassWpfComposition AuroraGlassWpfComposition;

typedef struct AuroraGlassWpfCompositionStats {
    uint64_t frameCount;
    uint32_t width;
    uint32_t height;
    int32_t lastCoreStatus;
    uint32_t ready;
} AuroraGlassWpfCompositionStats;

// Creates the offscreen composition (D3D11 device + D3D9Ex device + shared
// BGRA render texture). Returns nullptr on failure. No visible window is
// created; an internal hidden helper window is used only for D3D9Ex device
// creation (not a WS_CHILD window and never an output surface).
AURORAGLASS_WPF_COMP_API AuroraGlassWpfComposition*
AuroraGlassWpfCompositionCreate(
    uint32_t width,
    uint32_t height) AURORAGLASS_WPF_COMP_NOEXCEPT;

AURORAGLASS_WPF_COMP_API void
AuroraGlassWpfCompositionDestroy(
    AuroraGlassWpfComposition* composition) AURORAGLASS_WPF_COMP_NOEXCEPT;

// Returns the D3D9Ex level-0 IDirect3DSurface9* as an intptr_t, for
// D3DImage.SetBackBuffer(D3DResourceType.IDirect3DSurface9, ptr).
// Returns 0 if unavailable.
AURORAGLASS_WPF_COMP_API intptr_t
AuroraGlassWpfCompositionGetSurface(
    const AuroraGlassWpfComposition* composition) AURORAGLASS_WPF_COMP_NOEXCEPT;

// Resize the shared surface and internal resources. Physical pixels.
AURORAGLASS_WPF_COMP_API int32_t
AuroraGlassWpfCompositionResize(
    AuroraGlassWpfComposition* composition,
    uint32_t width,
    uint32_t height) AURORAGLASS_WPF_COMP_NOEXCEPT;

AURORAGLASS_WPF_COMP_API int32_t
AuroraGlassWpfCompositionSetMaterial(
    AuroraGlassWpfComposition* composition,
    const AuroraGlassWpfMaterialSnapshot* snapshot) AURORAGLASS_WPF_COMP_NOEXCEPT;

AURORAGLASS_WPF_COMP_API int32_t
AuroraGlassWpfCompositionSetRects(
    AuroraGlassWpfComposition* composition,
    const AuroraGlassWpfRenderRect* rects,
    uint32_t count) AURORAGLASS_WPF_COMP_NOEXCEPT;

// Renders one frame into the shared surface. timeSeconds drives the animation.
AURORAGLASS_WPF_COMP_API int32_t
AuroraGlassWpfCompositionRender(
    AuroraGlassWpfComposition* composition,
    float timeSeconds) AURORAGLASS_WPF_COMP_NOEXCEPT;

AURORAGLASS_WPF_COMP_API int32_t
AuroraGlassWpfCompositionGetStats(
    const AuroraGlassWpfComposition* composition,
    AuroraGlassWpfCompositionStats* outStats) AURORAGLASS_WPF_COMP_NOEXCEPT;

#if defined(__cplusplus)
    }
#endif

#undef AURORAGLASS_WPF_COMP_NOEXCEPT
