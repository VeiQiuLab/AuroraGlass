
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>
#include "glass_params.h"
#include "background_source.h"

namespace AuroraGlass {

class ShaderLibrary;

// P0 internal pipeline orchestrator.
// Renders: procedural background -> separable blur -> glass composite to backbuffer.
// All visual stages can be independently toggled via StageFlags.
class GlassRenderer {
public:
    bool Init(ID3D11Device* device, const std::wstring& shaderDir);
    void Release();
    bool Resize(ID3D11Device* device, uint32_t width, uint32_t height);

    // Render one frame to the given backbuffer RTV.
    void Render(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* backbufferRTV,
                uint32_t width, uint32_t height, float timeSeconds,
                const GlassMaterialParams& params, const StageFlags& stages);

    uint32_t Width() const { return width_; }
    uint32_t Height() const { return height_; }

private:
    // Shaders
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fsVS_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  blurPS_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  glassPS_;

    // Blur intermediate targets (ping-pong)
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          blurTexA_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   blurRTVA_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> blurSRVA_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          blurTexB_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   blurRTVB_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> blurSRVB_;

    // Constant buffers
    Microsoft::WRL::ComPtr<ID3D11Buffer> frameCB_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> materialCB_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> blurCB_;

    // Sampler
    Microsoft::WRL::ComPtr<ID3D11SamplerState> linearSampler_;

    // Background source
    BackgroundSource bgSource_;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

} // namespace AuroraGlass
