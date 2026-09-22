
#include "core/background_source.h"
#include "core/shader_library.h"

#include <cstring>

using Microsoft::WRL::ComPtr;

namespace AuroraGlass {

namespace {
// Must match FrameCB layout in shaders/background.hlsl (2 x float4 = 32 bytes).
struct FrameCBData {
    float resolution[4]; // xy = size, zw unused
    float time[4];       // x = seconds, yzw unused
};
}

bool BackgroundSource::Init(ID3D11Device* device, ShaderLibrary& shaders) {
    auto vsBlob = shaders.Compile(L"fullscreen_triangle.hlsl", "FullscreenVS", "vs_5_0");
    auto psBlob = shaders.Compile(L"background.hlsl", "BackgroundPS", "ps_5_0");
    if (!vsBlob || !psBlob) return false;

    if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                          nullptr, &vs_))) return false;
    if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                         nullptr, &ps_))) return false;

    D3D11_BUFFER_DESC cbd{};
    cbd.ByteWidth = sizeof(FrameCBData);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device->CreateBuffer(&cbd, nullptr, &frameCB_))) return false;

    return true;
}

void BackgroundSource::Release() {
    srv_.Reset(); rtv_.Reset(); texture_.Reset();
    vs_.Reset(); ps_.Reset(); frameCB_.Reset();
    width_ = height_ = 0;
}

bool BackgroundSource::Resize(ID3D11Device* device, uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return false;

    srv_.Reset(); rtv_.Reset(); texture_.Reset();

    D3D11_TEXTURE2D_DESC td{};
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(device->CreateTexture2D(&td, nullptr, &texture_))) return false;
    if (FAILED(device->CreateRenderTargetView(texture_.Get(), nullptr, &rtv_))) return false;
    if (FAILED(device->CreateShaderResourceView(texture_.Get(), nullptr, &srv_))) return false;

    width_ = width;
    height_ = height;
    return true;
}

void BackgroundSource::Render(ID3D11DeviceContext* ctx, float timeSeconds) {
    if (!texture_) return;

    FrameCBData cb{};
    cb.resolution[0] = static_cast<float>(width_);
    cb.resolution[1] = static_cast<float>(height_);
    cb.time[0] = timeSeconds;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(frameCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, &cb, sizeof(cb));
        ctx->Unmap(frameCB_.Get(), 0);
    }

    ctx->OMSetRenderTargets(1, rtv_.GetAddressOf(), nullptr);

    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(width_);
    vp.Height = static_cast<float>(height_);
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11Buffer* cbs[] = { frameCB_.Get() };
    ctx->VSSetConstantBuffers(0, 1, cbs);
    ctx->PSSetConstantBuffers(0, 1, cbs);

    ctx->VSSetShader(vs_.Get(), nullptr, 0);
    ctx->PSSetShader(ps_.Get(), nullptr, 0);
    ctx->Draw(3, 0);
}

} // namespace AuroraGlass
