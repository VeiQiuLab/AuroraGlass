#pragma once

#include "winui/winui_material_bridge.h"

#include <stdint.h>

#if defined(_WIN32)
    #if defined(AURORAGLASS_WINUI_INTEROP_EXPORTS)
        #define AURORAGLASS_WINUI_RENDER_API __declspec(dllexport)
    #else
        #define AURORAGLASS_WINUI_RENDER_API __declspec(dllimport)
    #endif
#else
    #define AURORAGLASS_WINUI_RENDER_API
#endif

#if defined(__cplusplus)
    #define AURORAGLASS_WINUI_RENDER_NOEXCEPT noexcept
    extern "C" {
#else
    #define AURORAGLASS_WINUI_RENDER_NOEXCEPT
#endif

typedef struct AuroraGlassWinUIRenderHost AuroraGlassWinUIRenderHost;

typedef struct AuroraGlassWinUIRenderRect {
    float x;
    float y;
    float width;
    float height;
} AuroraGlassWinUIRenderRect;

typedef struct AuroraGlassWinUIRenderStats {
    uint64_t frameCount;
    uint32_t width;
    uint32_t height;
    int32_t lastCoreStatus;
    uint32_t ready;
} AuroraGlassWinUIRenderStats;

AURORAGLASS_WINUI_RENDER_API AuroraGlassWinUIRenderHost*
AuroraGlassWinUIRenderHostCreate(
    intptr_t parentHwnd) AURORAGLASS_WINUI_RENDER_NOEXCEPT;

AURORAGLASS_WINUI_RENDER_API void
AuroraGlassWinUIRenderHostDestroy(
    AuroraGlassWinUIRenderHost* host) AURORAGLASS_WINUI_RENDER_NOEXCEPT;

AURORAGLASS_WINUI_RENDER_API intptr_t
AuroraGlassWinUIRenderHostGetHwnd(
    const AuroraGlassWinUIRenderHost* host) AURORAGLASS_WINUI_RENDER_NOEXCEPT;

AURORAGLASS_WINUI_RENDER_API int32_t
AuroraGlassWinUIRenderHostSetMaterial(
    AuroraGlassWinUIRenderHost* host,
    const AuroraGlassWinUIMaterialSnapshot* snapshot) AURORAGLASS_WINUI_RENDER_NOEXCEPT;

AURORAGLASS_WINUI_RENDER_API int32_t
AuroraGlassWinUIRenderHostSetRects(
    AuroraGlassWinUIRenderHost* host,
    const AuroraGlassWinUIRenderRect* rects,
    uint32_t count) AURORAGLASS_WINUI_RENDER_NOEXCEPT;

AURORAGLASS_WINUI_RENDER_API int32_t
AuroraGlassWinUIRenderHostGetStats(
    const AuroraGlassWinUIRenderHost* host,
    AuroraGlassWinUIRenderStats* outStats) AURORAGLASS_WINUI_RENDER_NOEXCEPT;


AURORAGLASS_WINUI_RENDER_API int32_t
AuroraGlassWinUIRenderHostResize(
    AuroraGlassWinUIRenderHost* host,
    uint32_t width,
    uint32_t height) AURORAGLASS_WINUI_RENDER_NOEXCEPT;
#if defined(__cplusplus)
    }
#endif

#undef AURORAGLASS_WINUI_RENDER_NOEXCEPT
