#pragma once

#include "wpf/wpf_material_bridge.h"

#include <stdint.h>

#if defined(_WIN32)
    #if defined(AURORAGLASS_WPF_INTEROP_EXPORTS)
        #define AURORAGLASS_WPF_RENDER_API __declspec(dllexport)
    #else
        #define AURORAGLASS_WPF_RENDER_API __declspec(dllimport)
    #endif
#else
    #define AURORAGLASS_WPF_RENDER_API
#endif

#if defined(__cplusplus)
    #define AURORAGLASS_WPF_RENDER_NOEXCEPT noexcept
    extern "C" {
#else
    #define AURORAGLASS_WPF_RENDER_NOEXCEPT
#endif

typedef struct AuroraGlassWpfRenderHost AuroraGlassWpfRenderHost;

typedef struct AuroraGlassWpfRenderRect {
    float x;
    float y;
    float width;
    float height;
} AuroraGlassWpfRenderRect;

typedef struct AuroraGlassWpfRenderStats {
    uint64_t frameCount;
    uint32_t width;
    uint32_t height;
    int32_t lastCoreStatus;
    uint32_t ready;
} AuroraGlassWpfRenderStats;

AURORAGLASS_WPF_RENDER_API AuroraGlassWpfRenderHost*
AuroraGlassWpfRenderHostCreate(
    intptr_t parentHwnd) AURORAGLASS_WPF_RENDER_NOEXCEPT;

AURORAGLASS_WPF_RENDER_API void
AuroraGlassWpfRenderHostDestroy(
    AuroraGlassWpfRenderHost* host) AURORAGLASS_WPF_RENDER_NOEXCEPT;

AURORAGLASS_WPF_RENDER_API intptr_t
AuroraGlassWpfRenderHostGetHwnd(
    const AuroraGlassWpfRenderHost* host) AURORAGLASS_WPF_RENDER_NOEXCEPT;

AURORAGLASS_WPF_RENDER_API int32_t
AuroraGlassWpfRenderHostSetMaterial(
    AuroraGlassWpfRenderHost* host,
    const AuroraGlassWpfMaterialSnapshot* snapshot) AURORAGLASS_WPF_RENDER_NOEXCEPT;

AURORAGLASS_WPF_RENDER_API int32_t
AuroraGlassWpfRenderHostSetRects(
    AuroraGlassWpfRenderHost* host,
    const AuroraGlassWpfRenderRect* rects,
    uint32_t count) AURORAGLASS_WPF_RENDER_NOEXCEPT;

AURORAGLASS_WPF_RENDER_API int32_t
AuroraGlassWpfRenderHostGetStats(
    const AuroraGlassWpfRenderHost* host,
    AuroraGlassWpfRenderStats* outStats) AURORAGLASS_WPF_RENDER_NOEXCEPT;

#if defined(__cplusplus)
    }
#endif

#undef AURORAGLASS_WPF_RENDER_NOEXCEPT
