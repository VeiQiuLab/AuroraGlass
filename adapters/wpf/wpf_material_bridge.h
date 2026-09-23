#pragma once

// ============================================================
// AuroraGlass P6 WPF Adapter - native interop boundary.
//
// This is a deliberately narrow P/Invoke-compatible C ABI over
// the frozen AuroraGlass GlassMaterial model.
//
// It does NOT provide:
//   - a WPF control framework
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
    #if defined(AURORAGLASS_WPF_INTEROP_EXPORTS)
        #define AURORAGLASS_WPF_API __declspec(dllexport)
    #else
        #define AURORAGLASS_WPF_API __declspec(dllimport)
    #endif
#else
    #define AURORAGLASS_WPF_API
#endif

#if defined(__cplusplus)
    #define AURORAGLASS_WPF_NOEXCEPT noexcept
    extern "C" {
#else
    #define AURORAGLASS_WPF_NOEXCEPT
#endif

typedef struct AuroraGlassWpfMaterial AuroraGlassWpfMaterial;

typedef struct AuroraGlassWpfMaterialSnapshot {
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
} AuroraGlassWpfMaterialSnapshot;

// Returns nullptr only when allocation fails.
AURORAGLASS_WPF_API AuroraGlassWpfMaterial*
AuroraGlassWpfMaterialCreate(void) AURORAGLASS_WPF_NOEXCEPT;

// Safe for nullptr.
AURORAGLASS_WPF_API void
AuroraGlassWpfMaterialDestroy(
    AuroraGlassWpfMaterial* material) AURORAGLASS_WPF_NOEXCEPT;

// Return value is the numeric AuroraGlass::ErrorCode.
AURORAGLASS_WPF_API int32_t
AuroraGlassWpfMaterialGetSnapshot(
    const AuroraGlassWpfMaterial* material,
    AuroraGlassWpfMaterialSnapshot* outSnapshot) AURORAGLASS_WPF_NOEXCEPT;

AURORAGLASS_WPF_API int32_t
AuroraGlassWpfMaterialSetBlurRadius(
    AuroraGlassWpfMaterial* material,
    float value) AURORAGLASS_WPF_NOEXCEPT;

AURORAGLASS_WPF_API int32_t
AuroraGlassWpfMaterialSetRefractionStrength(
    AuroraGlassWpfMaterial* material,
    float value) AURORAGLASS_WPF_NOEXCEPT;

AURORAGLASS_WPF_API int32_t
AuroraGlassWpfMaterialSetDispersionStrength(
    AuroraGlassWpfMaterial* material,
    float value) AURORAGLASS_WPF_NOEXCEPT;

AURORAGLASS_WPF_API int32_t
AuroraGlassWpfMaterialSetThickness(
    AuroraGlassWpfMaterial* material,
    float value) AURORAGLASS_WPF_NOEXCEPT;

AURORAGLASS_WPF_API int32_t
AuroraGlassWpfMaterialSetEdgeFresnel(
    AuroraGlassWpfMaterial* material,
    float value) AURORAGLASS_WPF_NOEXCEPT;

AURORAGLASS_WPF_API int32_t
AuroraGlassWpfMaterialSetSpecularStrength(
    AuroraGlassWpfMaterial* material,
    float value) AURORAGLASS_WPF_NOEXCEPT;

AURORAGLASS_WPF_API int32_t
AuroraGlassWpfMaterialSetTintAmount(
    AuroraGlassWpfMaterial* material,
    float value) AURORAGLASS_WPF_NOEXCEPT;

AURORAGLASS_WPF_API int32_t
AuroraGlassWpfMaterialSetSaturation(
    AuroraGlassWpfMaterial* material,
    float value) AURORAGLASS_WPF_NOEXCEPT;

AURORAGLASS_WPF_API int32_t
AuroraGlassWpfMaterialSetBrightness(
    AuroraGlassWpfMaterial* material,
    float value) AURORAGLASS_WPF_NOEXCEPT;

AURORAGLASS_WPF_API int32_t
AuroraGlassWpfMaterialSetNoiseAmount(
    AuroraGlassWpfMaterial* material,
    float value) AURORAGLASS_WPF_NOEXCEPT;

AURORAGLASS_WPF_API int32_t
AuroraGlassWpfMaterialSetCornerRadius(
    AuroraGlassWpfMaterial* material,
    float value) AURORAGLASS_WPF_NOEXCEPT;

AURORAGLASS_WPF_API int32_t
AuroraGlassWpfMaterialSetOpacity(
    AuroraGlassWpfMaterial* material,
    float value) AURORAGLASS_WPF_NOEXCEPT;

AURORAGLASS_WPF_API int32_t
AuroraGlassWpfMaterialSetHighlightPosition(
    AuroraGlassWpfMaterial* material,
    float x,
    float y) AURORAGLASS_WPF_NOEXCEPT;

#if defined(__cplusplus)
    }
#endif

#undef AURORAGLASS_WPF_NOEXCEPT
