#pragma once

// ============================================================
// AuroraGlass P7 WinUI 3 Adapter - native interop boundary.
//
// This is a deliberately narrow P/Invoke-compatible C ABI over
// the frozen AuroraGlass GlassMaterial model.
//
// It does NOT provide:
//   - a WinUI control framework
//   - layout
//   - XAML infrastructure
//   - MVVM/data binding
//   - window management
//   - a second material model
//
// Managed consumers own only the opaque handle returned here.
// All material semantics remain implemented by GlassMaterial.
// ============================================================

#include <stdint.h>

#if defined(_WIN32)
    #if defined(AURORAGLASS_WINUI_INTEROP_EXPORTS)
        #define AURORAGLASS_WINUI_API __declspec(dllexport)
    #else
        #define AURORAGLASS_WINUI_API __declspec(dllimport)
    #endif
#else
    #define AURORAGLASS_WINUI_API
#endif

#if defined(__cplusplus)
    #define AURORAGLASS_WINUI_NOEXCEPT noexcept
    extern "C" {
#else
    #define AURORAGLASS_WINUI_NOEXCEPT
#endif

typedef struct AuroraGlassWinUIMaterial AuroraGlassWinUIMaterial;

typedef struct AuroraGlassWinUIMaterialSnapshot {
    float blurRadius;
    float refractionStrength;
    float dispersionStrength;
    float thickness;
    float edgeFresnel;
    float specularStrength;
    float tintAmount;
    float saturation;
    float brightness;
    float noiseAmount;
    float cornerRadius;
    float opacity;
    float highlightX;
    float highlightY;
} AuroraGlassWinUIMaterialSnapshot;

// Returns nullptr only when allocation fails.
AURORAGLASS_WINUI_API AuroraGlassWinUIMaterial*
AuroraGlassWinUIMaterialCreate(void) AURORAGLASS_WINUI_NOEXCEPT;

// Safe for nullptr.
AURORAGLASS_WINUI_API void
AuroraGlassWinUIMaterialDestroy(
    AuroraGlassWinUIMaterial* material) AURORAGLASS_WINUI_NOEXCEPT;

// Return value is the numeric AuroraGlass::ErrorCode.
AURORAGLASS_WINUI_API int32_t
AuroraGlassWinUIMaterialGetSnapshot(
    const AuroraGlassWinUIMaterial* material,
    AuroraGlassWinUIMaterialSnapshot* outSnapshot) AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API int32_t
AuroraGlassWinUIMaterialSetBlurRadius(
    AuroraGlassWinUIMaterial* material,
    float value) AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API int32_t
AuroraGlassWinUIMaterialSetRefractionStrength(
    AuroraGlassWinUIMaterial* material,
    float value) AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API int32_t
AuroraGlassWinUIMaterialSetDispersionStrength(
    AuroraGlassWinUIMaterial* material,
    float value) AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API int32_t
AuroraGlassWinUIMaterialSetThickness(
    AuroraGlassWinUIMaterial* material,
    float value) AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API int32_t
AuroraGlassWinUIMaterialSetEdgeFresnel(
    AuroraGlassWinUIMaterial* material,
    float value) AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API int32_t
AuroraGlassWinUIMaterialSetSpecularStrength(
    AuroraGlassWinUIMaterial* material,
    float value) AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API int32_t
AuroraGlassWinUIMaterialSetTintAmount(
    AuroraGlassWinUIMaterial* material,
    float value) AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API int32_t
AuroraGlassWinUIMaterialSetSaturation(
    AuroraGlassWinUIMaterial* material,
    float value) AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API int32_t
AuroraGlassWinUIMaterialSetBrightness(
    AuroraGlassWinUIMaterial* material,
    float value) AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API int32_t
AuroraGlassWinUIMaterialSetNoiseAmount(
    AuroraGlassWinUIMaterial* material,
    float value) AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API int32_t
AuroraGlassWinUIMaterialSetCornerRadius(
    AuroraGlassWinUIMaterial* material,
    float value) AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API int32_t
AuroraGlassWinUIMaterialSetOpacity(
    AuroraGlassWinUIMaterial* material,
    float value) AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API int32_t
AuroraGlassWinUIMaterialSetHighlightPosition(
    AuroraGlassWinUIMaterial* material,
    float x,
    float y) AURORAGLASS_WINUI_NOEXCEPT;

#if defined(__cplusplus)
    }
#endif

#undef AURORAGLASS_WINUI_NOEXCEPT
