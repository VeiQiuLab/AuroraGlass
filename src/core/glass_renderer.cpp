
#include "core/glass_renderer.h"
#include "core/shader_library.h"

#include <cmath>

using Microsoft::WRL::ComPtr;

namespace AuroraGlass {

// ============================================================
// Constant buffer layouts (must match HLSL exactly)
// ============================================================

// FrameCB: 32 bytes
struct FrameCBData {
    float resolution[4]; // xy = size, zw unused
    float time[4];       // x = seconds, yzw unused
};

// MaterialCB: 112 bytes (7 x float4)
struct MaterialCBData {
    float m_A[4];        // blurRadius, refraction, dispersion, thickness
    float m_B[4];        // edgeFresnel, specular, tintAmount, saturation
    float m_C[4];        // brightness, noiseAmount, cornerRadius, opacity
    float m_D[4];        // centerX, centerY, halfW, halfH
    float m_E[4];        // highlightX, highlightY, zw unused
    float m_Stages[4];   // refraction, dispersion, fresnel, specular
    float m_Stages2[4];  // mask, colorAdjust, zw unused
};

// BlurCB: 32 bytes
struct BlurCBData {
    float texel[4];   // x=1/w, y=1/h, z=dir.x, w=dir.y
    float param[4];   // x=radius, yzw unused
};

// ============================================================
// Init
// ============================================================

bool GlassRenderer::Init(ID3D11Device* device, const std::wstring& shaderDir) {
    ShaderLibrary shaders;
    shaders.Init(shaderDir);

    // Compile fullscreen VS (shared by all passes)
    auto vsBlob = shaders.Compile(L"fullscreen_triangle.hlsl", "FullscreenVS", "vs_5_0");
    if (!vsBlob) return false;
    if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                          nullptr, &fsVS_))) return false;

    // Compile blur PS
    auto blurBlob = shaders.Compile(L"blur.hlsl", "BlurPS", "ps_5_0");
    if (!blurBlob) return false;
    if (FAILED(device->CreatePixelShader(blurBlob->GetBufferPointer(), blurBlob->GetBufferSize(),
                                         nullptr, &blurPS_))) return false;

    // Compile glass PS
    auto glassBlob = shaders.Compile(L"glass.hlsl", "GlassPS", "ps_5_0");
    if (!glassBlob) return false;
    if (FAILED(device->CreatePixelShader(glassBlob->GetBufferPointer(), glassBlob->GetBufferSize(),
                                         nullptr, &glassPS_))) return false;

    // Create constant buffers
    D3D11_BUFFER_DESC cbd{};
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    cbd.ByteWidth = sizeof(FrameCBData);
    if (FAILED(device->CreateBuffer(&cbd, nullptr, &frameCB_))) return false;

    cbd.ByteWidth = sizeof(MaterialCBData);
    if (FAILED(device->CreateBuffer(&cbd, nullptr, &materialCB_))) return false;

    cbd.ByteWidth = sizeof(BlurCBData);
    if (FAILED(device->CreateBuffer(&cbd, nullptr, &blurCB_))) return false;

    // Create linear sampler
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device->CreateSamplerState(&sd, &linearSampler_))) return false;

    // Init background source
    if (!bgSource_.Init(device, shaders)) return false;

    return true;
}

void GlassRenderer::Release() {
    bgSource_.Release();
    blurSRVB_.Reset(); blurRTVB_.Reset(); blurTexB_.Reset();
    blurSRVA_.Reset(); blurRTVA_.Reset(); blurTexA_.Reset();
    linearSampler_.Reset();
    blurCB_.Reset(); materialCB_.Reset(); frameCB_.Reset();
    glassPS_.Reset(); blurPS_.Reset(); fsVS_.Reset();
    width_ = height_ = 0;
}

// ============================================================
// Resize
// ============================================================

bool GlassRenderer::Resize(ID3D11Device* device, uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return false;

    // Resize background source
    if (!bgSource_.Resize(device, width, height)) return false;

    // Recreate blur targets
    blurSRVB_.Reset(); blurRTVB_.Reset(); blurTexB_.Reset();
    blurSRVA_.Reset(); blurRTVA_.Reset(); blurTexA_.Reset();

    D3D11_TEXTURE2D_DESC td{};
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(device->CreateTexture2D(&td, nullptr, &blurTexA_))) return false;
    if (FAILED(device->CreateRenderTargetView(blurTexA_.Get(), nullptr, &blurRTVA_))) return false;
    if (FAILED(device->CreateShaderResourceView(blurTexA_.Get(), nullptr, &blurSRVA_))) return false;

    if (FAILED(device->CreateTexture2D(&td, nullptr, &blurTexB_))) return false;
    if (FAILED(device->CreateRenderTargetView(blurTexB_.Get(), nullptr, &blurRTVB_))) return false;
    if (FAILED(device->CreateShaderResourceView(blurTexB_.Get(), nullptr, &blurSRVB_))) return false;

    width_ = width;
    height_ = height;
    return true;
}

// ============================================================
// Render
// ============================================================

void GlassRenderer::Render(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* backbufferRTV,
                           uint32_t width, uint32_t height, float timeSeconds,
                           const GlassMaterialParams& params, const StageFlags& stages) {
    if (!fsVS_ || width == 0 || height == 0) return;

    // ---- Pass 1: Background ----
    bgSource_.Render(ctx, timeSeconds);

    // ---- Pass 2 & 3: Blur (horizontal + vertical) ----
    ID3D11ShaderResourceView* blurredSRV = bgSource_.TextureSRV();

    if (stages.blur) {
        // Horizontal: bg -> blurA
        {
            D3D11_VIEWPORT vp{};
            vp.Width = (float)width; vp.Height = (float)height; vp.MaxDepth = 1.0f;
            ctx->OMSetRenderTargets(1, blurRTVA_.GetAddressOf(), nullptr);
            ctx->RSSetViewports(1, &vp);
            ctx->IASetInputLayout(nullptr);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->VSSetShader(fsVS_.Get(), nullptr, 0);
            ctx->PSSetShader(blurPS_.Get(), nullptr, 0);

            BlurCBData cb{};
            cb.texel[0] = 1.0f / width;
            cb.texel[1] = 1.0f / height;
            cb.texel[2] = 1.0f; // dir.x
            cb.texel[3] = 0.0f; // dir.y
            cb.param[0] = params.blurRadius;

            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (SUCCEEDED(ctx->Map(blurCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                memcpy(mapped.pData, &cb, sizeof(cb));
                ctx->Unmap(blurCB_.Get(), 0);
            }
            ctx->PSSetConstantBuffers(0, 1, blurCB_.GetAddressOf());

            ID3D11ShaderResourceView* srvs[] = { bgSource_.TextureSRV() };
            ctx->PSSetShaderResources(0, 1, srvs);
            ctx->PSSetSamplers(0, 1, linearSampler_.GetAddressOf());
            ctx->Draw(3, 0);

            // Unbind SRV before using as RT in next pass
            ID3D11ShaderResourceView* nullSRV[] = { nullptr };
            ctx->PSSetShaderResources(0, 1, nullSRV);
        }

        // Vertical: blurA -> blurB
        {
            ctx->OMSetRenderTargets(1, blurRTVB_.GetAddressOf(), nullptr);
            ctx->IASetInputLayout(nullptr);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->VSSetShader(fsVS_.Get(), nullptr, 0);
            ctx->PSSetShader(blurPS_.Get(), nullptr, 0);

            BlurCBData cb{};
            cb.texel[0] = 1.0f / width;
            cb.texel[1] = 1.0f / height;
            cb.texel[2] = 0.0f; // dir.x
            cb.texel[3] = 1.0f; // dir.y
            cb.param[0] = params.blurRadius;

            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (SUCCEEDED(ctx->Map(blurCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                memcpy(mapped.pData, &cb, sizeof(cb));
                ctx->Unmap(blurCB_.Get(), 0);
            }
            ctx->PSSetConstantBuffers(0, 1, blurCB_.GetAddressOf());

            ID3D11ShaderResourceView* srvs[] = { blurSRVA_.Get() };
            ctx->PSSetShaderResources(0, 1, srvs);
            ctx->PSSetSamplers(0, 1, linearSampler_.GetAddressOf());
            ctx->Draw(3, 0);

            ID3D11ShaderResourceView* nullSRV[] = { nullptr };
            ctx->PSSetShaderResources(0, 1, nullSRV);
        }

        blurredSRV = blurSRVB_.Get();
    }

    // ---- Pass 4: Glass composite -> backbuffer ----
    {
        D3D11_VIEWPORT vp{};
        vp.Width = (float)width; vp.Height = (float)height; vp.MaxDepth = 1.0f;
        ctx->OMSetRenderTargets(1, &backbufferRTV, nullptr);
        ctx->RSSetViewports(1, &vp);
        ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(fsVS_.Get(), nullptr, 0);
        ctx->PSSetShader(glassPS_.Get(), nullptr, 0);

        // FrameCB
        FrameCBData frameCB{};
        frameCB.resolution[0] = (float)width;
        frameCB.resolution[1] = (float)height;
        frameCB.time[0] = timeSeconds;
        {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (SUCCEEDED(ctx->Map(frameCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                memcpy(mapped.pData, &frameCB, sizeof(frameCB));
                ctx->Unmap(frameCB_.Get(), 0);
            }
        }

        // MaterialCB
        MaterialCBData matCB{};
        // m_A: blurRadius, refraction, dispersion, thickness
        matCB.m_A[0] = params.blurRadius;
        matCB.m_A[1] = params.refractionStrength;
        matCB.m_A[2] = params.dispersionStrength;
        matCB.m_A[3] = params.thickness;
        // m_B: edgeFresnel, specular, tintAmount, saturation
        matCB.m_B[0] = params.edgeFresnel;
        matCB.m_B[1] = params.specularStrength;
        matCB.m_B[2] = params.tintAmount;
        matCB.m_B[3] = params.saturation;
        // m_C: brightness, noiseAmount, cornerRadius, opacity
        matCB.m_C[0] = params.brightness;
        matCB.m_C[1] = params.noiseAmount;
        matCB.m_C[2] = params.cornerRadius;
        matCB.m_C[3] = params.opacity;
        // m_D: centerX, centerY, halfW, halfH
        matCB.m_D[0] = width * 0.5f;
        matCB.m_D[1] = height * 0.5f;
        matCB.m_D[2] = width * 0.30f;
        matCB.m_D[3] = height * 0.30f;
        // m_E: highlightX, highlightY
        matCB.m_E[0] = params.highlightPos[0];
        matCB.m_E[1] = params.highlightPos[1];
        // m_Stages: refraction, dispersion, fresnel, specular
        matCB.m_Stages[0] = stages.refraction ? 1.0f : 0.0f;
        matCB.m_Stages[1] = stages.dispersion ? 1.0f : 0.0f;
        matCB.m_Stages[2] = stages.fresnel ? 1.0f : 0.0f;
        matCB.m_Stages[3] = stages.specular ? 1.0f : 0.0f;
        // m_Stages2: mask, colorAdjust
        matCB.m_Stages2[0] = stages.mask ? 1.0f : 0.0f;
        matCB.m_Stages2[1] = stages.colorAdjust ? 1.0f : 0.0f;

        {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (SUCCEEDED(ctx->Map(materialCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                memcpy(mapped.pData, &matCB, sizeof(matCB));
                ctx->Unmap(materialCB_.Get(), 0);
            }
        }

        ID3D11Buffer* cbs[] = { frameCB_.Get(), materialCB_.Get() };
        ctx->PSSetConstantBuffers(0, 2, cbs);

        ID3D11ShaderResourceView* srvs[] = { bgSource_.TextureSRV(), blurredSRV };
        ctx->PSSetShaderResources(0, 2, srvs);
        ctx->PSSetSamplers(0, 1, linearSampler_.GetAddressOf());

        ctx->Draw(3, 0);
    }
}

} // namespace AuroraGlass
