#include "wpf/wpf_material_bridge.h"

#include <Windows.h>

#include <cmath>
#include <cstdio>
#include <limits>

namespace {

int g_checks = 0;
int g_failures = 0;

void Check(bool condition, const char* name) {
    ++g_checks;

    if (!condition) {
        ++g_failures;
        std::printf("[FAIL] %s\n", name);
    }
}

bool Near(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

void CheckExport(
    HMODULE module,
    const char* name)
{
    Check(
        GetProcAddress(module, name) != nullptr,
        name);
}

} // namespace

int main() {
    static_assert(
        sizeof(AuroraGlassWpfMaterialSnapshot) ==
            sizeof(float) * 14,
        "WPF material snapshot must remain a flat 14-float ABI.");

    AuroraGlassWpfMaterial* material =
        AuroraGlassWpfMaterialCreate();

    Check(
        material != nullptr,
        "create material");

    if (!material) {
        std::printf(
            "P6_WPF_INTEROP_TESTS: %d checks, %d failures\n",
            g_checks,
            g_failures);

        return 1;
    }

    HMODULE module =
        GetModuleHandleW(
            L"AuroraGlassWpfInterop.dll");

    Check(
        module != nullptr,
        "interop DLL loaded");

    if (module) {
        CheckExport(
            module,
            "AuroraGlassWpfMaterialCreate");

        CheckExport(
            module,
            "AuroraGlassWpfMaterialDestroy");

        CheckExport(
            module,
            "AuroraGlassWpfMaterialGetSnapshot");

        CheckExport(
            module,
            "AuroraGlassWpfMaterialSetBlurRadius");

        CheckExport(
            module,
            "AuroraGlassWpfMaterialSetOpacity");

        CheckExport(
            module,
            "AuroraGlassWpfMaterialSetHighlightPosition");
    }

    AuroraGlassWpfMaterialSnapshot initial{};

    Check(
        AuroraGlassWpfMaterialGetSnapshot(
            material,
            &initial) == 0,
        "get default snapshot");

    Check(
        Near(initial.blurRadius, 12.0f),
        "default blur");

    Check(
        Near(initial.refractionStrength, 0.60f),
        "default refraction");

    Check(
        Near(initial.dispersionStrength, 0.50f),
        "default dispersion");

    Check(
        Near(initial.thickness, 0.50f),
        "default thickness");

    Check(
        Near(initial.edgeFresnel, 0.60f),
        "default fresnel");

    Check(
        Near(initial.specularStrength, 1.00f),
        "default specular");

    Check(
        Near(initial.tintAmount, 0.25f),
        "default tint");

    Check(
        Near(initial.saturation, 1.05f),
        "default saturation");

    Check(
        Near(initial.brightness, 1.02f),
        "default brightness");

    Check(
        Near(initial.noiseAmount, 0.03f),
        "default noise");

    Check(
        Near(initial.cornerRadius, 28.0f),
        "default corner radius");

    Check(
        Near(initial.opacity, 0.92f),
        "default opacity");

    Check(
        Near(initial.highlightX, 0.50f) &&
        Near(initial.highlightY, 0.35f),
        "default highlight");

    Check(
        AuroraGlassWpfMaterialSetBlurRadius(
            material,
            18.0f) == 0,
        "set blur");

    Check(
        AuroraGlassWpfMaterialSetRefractionStrength(
            material,
            0.40f) == 0,
        "set refraction");

    Check(
        AuroraGlassWpfMaterialSetDispersionStrength(
            material,
            0.30f) == 0,
        "set dispersion");

    Check(
        AuroraGlassWpfMaterialSetThickness(
            material,
            0.70f) == 0,
        "set thickness");

    Check(
        AuroraGlassWpfMaterialSetEdgeFresnel(
            material,
            0.45f) == 0,
        "set fresnel");

    Check(
        AuroraGlassWpfMaterialSetSpecularStrength(
            material,
            1.25f) == 0,
        "set specular");

    Check(
        AuroraGlassWpfMaterialSetTintAmount(
            material,
            0.15f) == 0,
        "set tint");

    Check(
        AuroraGlassWpfMaterialSetSaturation(
            material,
            1.20f) == 0,
        "set saturation");

    Check(
        AuroraGlassWpfMaterialSetBrightness(
            material,
            1.10f) == 0,
        "set brightness");

    Check(
        AuroraGlassWpfMaterialSetNoiseAmount(
            material,
            0.05f) == 0,
        "set noise");

    Check(
        AuroraGlassWpfMaterialSetCornerRadius(
            material,
            36.0f) == 0,
        "set corner radius");

    Check(
        AuroraGlassWpfMaterialSetOpacity(
            material,
            0.80f) == 0,
        "set opacity");

    Check(
        AuroraGlassWpfMaterialSetHighlightPosition(
            material,
            -0.25f,
            0.75f) == 0,
        "set highlight");

    AuroraGlassWpfMaterialSnapshot changed{};

    Check(
        AuroraGlassWpfMaterialGetSnapshot(
            material,
            &changed) == 0,
        "get changed snapshot");

    Check(
        Near(changed.blurRadius, 18.0f),
        "changed blur");

    Check(
        Near(changed.opacity, 0.80f),
        "changed opacity");

    Check(
        Near(changed.highlightX, -0.25f) &&
        Near(changed.highlightY, 0.75f),
        "changed highlight");

    const float nan =
        std::numeric_limits<float>::quiet_NaN();

    Check(
        AuroraGlassWpfMaterialSetOpacity(
            material,
            nan) == 1,
        "NaN maps to InvalidArgument");

    AuroraGlassWpfMaterialSnapshot afterInvalid{};

    Check(
        AuroraGlassWpfMaterialGetSnapshot(
            material,
            &afterInvalid) == 0,
        "snapshot after invalid setter");

    Check(
        Near(afterInvalid.opacity, 0.80f),
        "invalid setter preserves value");

    Check(
        AuroraGlassWpfMaterialGetSnapshot(
            nullptr,
            &afterInvalid) == 1,
        "null material rejected");

    Check(
        AuroraGlassWpfMaterialGetSnapshot(
            material,
            nullptr) == 1,
        "null snapshot rejected");

    Check(
        AuroraGlassWpfMaterialSetBlurRadius(
            nullptr,
            1.0f) == 1,
        "null setter rejected");

    AuroraGlassWpfMaterialDestroy(material);
    AuroraGlassWpfMaterialDestroy(nullptr);

    std::printf(
        "P6_WPF_INTEROP_TESTS: %d checks, %d failures\n",
        g_checks,
        g_failures);

    return g_failures == 0 ? 0 : 1;
}
