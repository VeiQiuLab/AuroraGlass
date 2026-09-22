
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>

namespace AuroraGlass {

class ShaderLibrary;

// Minimal background input abstraction.
//
// P0 scope: a procedural, animated test background rendered into an offscreen
// texture that the glass pipeline samples. This deliberately does NOT introduce
// desktop duplication, window capture, screen capture, or any provider/plugin
// hierarchy. The only contract the renderer relies on is:
//   Render() produces a full-size background texture, exposed via TextureSRV().
class BackgroundSource {
public:
    bool Init(ID3D11Device* device, ShaderLibrary& shaders);
    void Release();

    // Recreate offscreen targets on resize.
    bool Resize(ID3D11Device* device, uint32_t width, uint32_t height);

    // Render the animated background for the current frame.
    void Render(ID3D11DeviceContext* ctx, float timeSeconds);

    ID3D11ShaderResourceView* TextureSRV() const { return srv_.Get(); }
    uint32_t Width() const { return width_; }
    uint32_t Height() const { return height_; }

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D>           texture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>    rtv_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  srv_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>        vs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>         ps_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>              frameCB_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

} // namespace AuroraGlass
