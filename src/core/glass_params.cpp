
#include "core/glass_params.h"

namespace AuroraGlass {

namespace {
float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
}

void GlassMaterialParams::Clamp() {
    blurRadius         = clampf(blurRadius, 0.0f, 24.0f);
    refractionStrength = clampf(refractionStrength, 0.0f, 1.0f);
    dispersionStrength = clampf(dispersionStrength, 0.0f, 1.0f);
    thickness          = clampf(thickness, 0.0f, 1.0f);
    edgeFresnel        = clampf(edgeFresnel, 0.0f, 1.0f);
    specularStrength   = clampf(specularStrength, 0.0f, 2.0f);
    tintAmount         = clampf(tintAmount, 0.0f, 1.0f);
    saturation         = clampf(saturation, 0.0f, 2.0f);
    brightness         = clampf(brightness, 0.0f, 2.0f);
    noiseAmount        = clampf(noiseAmount, 0.0f, 0.2f);
    cornerRadius       = clampf(cornerRadius, 0.0f, 200.0f);
    opacity            = clampf(opacity, 0.0f, 1.0f);
    highlightPos[0]    = clampf(highlightPos[0], -1.0f, 1.0f);
    highlightPos[1]    = clampf(highlightPos[1], -1.0f, 1.0f);
}

} // namespace AuroraGlass
