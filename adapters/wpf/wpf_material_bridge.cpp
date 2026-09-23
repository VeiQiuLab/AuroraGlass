#include "wpf/wpf_material_bridge.h"

#include "core/glass_material.h"
#include "core/result.h"

#include <new>

using AuroraGlass::ErrorCode;
using AuroraGlass::GlassMaterial;
using AuroraGlass::Status;

struct AuroraGlassWpfMaterial {
    GlassMaterial value{};
};

namespace {

int32_t ToInteropCode(Status status) noexcept {
    return static_cast<int32_t>(status.code);
}

int32_t InvalidArgumentCode() noexcept {
    return static_cast<int32_t>(ErrorCode::InvalidArgument);
}

bool IsValid(
    const AuroraGlassWpfMaterial* material) noexcept
{
    return material != nullptr;
}

} // namespace

extern "C" {

AuroraGlassWpfMaterial*
AuroraGlassWpfMaterialCreate(void) noexcept {
    return new (std::nothrow) AuroraGlassWpfMaterial{};
}

void AuroraGlassWpfMaterialDestroy(
    AuroraGlassWpfMaterial* material) noexcept
{
    delete material;
}

int32_t AuroraGlassWpfMaterialGetSnapshot(
    const AuroraGlassWpfMaterial* material,
    AuroraGlassWpfMaterialSnapshot* outSnapshot) noexcept
{
    if (!IsValid(material) || outSnapshot == nullptr) {
        return InvalidArgumentCode();
    }

    outSnapshot->blurRadius =
        material->value.GetBlurRadius();

    outSnapshot->refractionStrength =
        material->value.GetRefractionStrength();

    outSnapshot->dispersionStrength =
        material->value.GetDispersionStrength();

    outSnapshot->thickness =
        material->value.GetThickness();

    outSnapshot->edgeFresnel =
        material->value.GetEdgeFresnel();

    outSnapshot->specularStrength =
        material->value.GetSpecularStrength();

    outSnapshot->tintAmount =
        material->value.GetTintAmount();

    outSnapshot->saturation =
        material->value.GetSaturation();

    outSnapshot->brightness =
        material->value.GetBrightness();

    outSnapshot->noiseAmount =
        material->value.GetNoiseAmount();

    outSnapshot->cornerRadius =
        material->value.GetCornerRadius();

    outSnapshot->opacity =
        material->value.GetOpacity();

    material->value.GetHighlightPosition(
        outSnapshot->highlightX,
        outSnapshot->highlightY);

    return static_cast<int32_t>(ErrorCode::Ok);
}

int32_t AuroraGlassWpfMaterialSetBlurRadius(
    AuroraGlassWpfMaterial* material,
    float value) noexcept
{
    if (!IsValid(material)) {
        return InvalidArgumentCode();
    }

    return ToInteropCode(
        material->value.SetBlurRadius(value));
}

int32_t AuroraGlassWpfMaterialSetRefractionStrength(
    AuroraGlassWpfMaterial* material,
    float value) noexcept
{
    if (!IsValid(material)) {
        return InvalidArgumentCode();
    }

    return ToInteropCode(
        material->value.SetRefractionStrength(value));
}

int32_t AuroraGlassWpfMaterialSetDispersionStrength(
    AuroraGlassWpfMaterial* material,
    float value) noexcept
{
    if (!IsValid(material)) {
        return InvalidArgumentCode();
    }

    return ToInteropCode(
        material->value.SetDispersionStrength(value));
}

int32_t AuroraGlassWpfMaterialSetThickness(
    AuroraGlassWpfMaterial* material,
    float value) noexcept
{
    if (!IsValid(material)) {
        return InvalidArgumentCode();
    }

    return ToInteropCode(
        material->value.SetThickness(value));
}

int32_t AuroraGlassWpfMaterialSetEdgeFresnel(
    AuroraGlassWpfMaterial* material,
    float value) noexcept
{
    if (!IsValid(material)) {
        return InvalidArgumentCode();
    }

    return ToInteropCode(
        material->value.SetEdgeFresnel(value));
}

int32_t AuroraGlassWpfMaterialSetSpecularStrength(
    AuroraGlassWpfMaterial* material,
    float value) noexcept
{
    if (!IsValid(material)) {
        return InvalidArgumentCode();
    }

    return ToInteropCode(
        material->value.SetSpecularStrength(value));
}

int32_t AuroraGlassWpfMaterialSetTintAmount(
    AuroraGlassWpfMaterial* material,
    float value) noexcept
{
    if (!IsValid(material)) {
        return InvalidArgumentCode();
    }

    return ToInteropCode(
        material->value.SetTintAmount(value));
}

int32_t AuroraGlassWpfMaterialSetSaturation(
    AuroraGlassWpfMaterial* material,
    float value) noexcept
{
    if (!IsValid(material)) {
        return InvalidArgumentCode();
    }

    return ToInteropCode(
        material->value.SetSaturation(value));
}

int32_t AuroraGlassWpfMaterialSetBrightness(
    AuroraGlassWpfMaterial* material,
    float value) noexcept
{
    if (!IsValid(material)) {
        return InvalidArgumentCode();
    }

    return ToInteropCode(
        material->value.SetBrightness(value));
}

int32_t AuroraGlassWpfMaterialSetNoiseAmount(
    AuroraGlassWpfMaterial* material,
    float value) noexcept
{
    if (!IsValid(material)) {
        return InvalidArgumentCode();
    }

    return ToInteropCode(
        material->value.SetNoiseAmount(value));
}

int32_t AuroraGlassWpfMaterialSetCornerRadius(
    AuroraGlassWpfMaterial* material,
    float value) noexcept
{
    if (!IsValid(material)) {
        return InvalidArgumentCode();
    }

    return ToInteropCode(
        material->value.SetCornerRadius(value));
}

int32_t AuroraGlassWpfMaterialSetOpacity(
    AuroraGlassWpfMaterial* material,
    float value) noexcept
{
    if (!IsValid(material)) {
        return InvalidArgumentCode();
    }

    return ToInteropCode(
        material->value.SetOpacity(value));
}

int32_t AuroraGlassWpfMaterialSetHighlightPosition(
    AuroraGlassWpfMaterial* material,
    float x,
    float y) noexcept
{
    if (!IsValid(material)) {
        return InvalidArgumentCode();
    }

    return ToInteropCode(
        material->value.SetHighlightPosition(x, y));
}

} // extern "C"
