#include "winui/winui_material_bridge.h"
#include "wpf/wpf_material_bridge.h"

#include <new>

struct AuroraGlassWinUIMaterial {
    AuroraGlassWpfMaterial* inner = nullptr;
};

extern "C" {

AuroraGlassWinUIMaterial*
AuroraGlassWinUIMaterialCreate(void) noexcept
{
    auto* wrapper =
        new (std::nothrow) AuroraGlassWinUIMaterial{};

    if (wrapper == nullptr) {
        return nullptr;
    }

    wrapper->inner =
        AuroraGlassWpfMaterialCreate();

    if (wrapper->inner == nullptr) {
        delete wrapper;
        return nullptr;
    }

    return wrapper;
}

void AuroraGlassWinUIMaterialDestroy(
    AuroraGlassWinUIMaterial* material) noexcept
{
    if (material == nullptr) {
        return;
    }

    AuroraGlassWpfMaterialDestroy(
        material->inner);

    delete material;
}

int32_t AuroraGlassWinUIMaterialGetSnapshot(
    const AuroraGlassWinUIMaterial* material,
    AuroraGlassWinUIMaterialSnapshot* outSnapshot) noexcept
{
    if (outSnapshot == nullptr) {
        return AuroraGlassWpfMaterialGetSnapshot(
            material != nullptr ? material->inner : nullptr,
            nullptr);
    }

    AuroraGlassWpfMaterialSnapshot source{};

    const int32_t status =
        AuroraGlassWpfMaterialGetSnapshot(
            material != nullptr ? material->inner : nullptr,
            &source);

    if (status != 0) {
        return status;
    }

    outSnapshot->blurRadius = source.blurRadius;
    outSnapshot->refractionStrength = source.refractionStrength;
    outSnapshot->dispersionStrength = source.dispersionStrength;
    outSnapshot->thickness = source.thickness;
    outSnapshot->edgeFresnel = source.edgeFresnel;
    outSnapshot->specularStrength = source.specularStrength;
    outSnapshot->tintAmount = source.tintAmount;
    outSnapshot->saturation = source.saturation;
    outSnapshot->brightness = source.brightness;
    outSnapshot->noiseAmount = source.noiseAmount;
    outSnapshot->cornerRadius = source.cornerRadius;
    outSnapshot->opacity = source.opacity;
    outSnapshot->highlightX = source.highlightX;
    outSnapshot->highlightY = source.highlightY;

    return 0;
}

#define FORWARD_SETTER(Name) \
int32_t AuroraGlassWinUIMaterialSet##Name( \
    AuroraGlassWinUIMaterial* material, \
    float value) noexcept \
{ \
    return AuroraGlassWpfMaterialSet##Name( \
        material != nullptr ? material->inner : nullptr, \
        value); \
}

FORWARD_SETTER(BlurRadius)
FORWARD_SETTER(RefractionStrength)
FORWARD_SETTER(DispersionStrength)
FORWARD_SETTER(Thickness)
FORWARD_SETTER(EdgeFresnel)
FORWARD_SETTER(SpecularStrength)
FORWARD_SETTER(TintAmount)
FORWARD_SETTER(Saturation)
FORWARD_SETTER(Brightness)
FORWARD_SETTER(NoiseAmount)
FORWARD_SETTER(CornerRadius)
FORWARD_SETTER(Opacity)

#undef FORWARD_SETTER

int32_t AuroraGlassWinUIMaterialSetHighlightPosition(
    AuroraGlassWinUIMaterial* material,
    float x,
    float y) noexcept
{
    return AuroraGlassWpfMaterialSetHighlightPosition(
        material != nullptr ? material->inner : nullptr,
        x,
        y);
}

}