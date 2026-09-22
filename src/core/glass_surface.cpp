#include "core/glass_surface.h"
#include "core/shader_library.h"

#include <cstring>
#include <cmath>
#include <filesystem>
#include <vector>
#include <Windows.h>

using Microsoft::WRL::ComPtr;

namespace AuroraGlass {

namespace {

// Internal shader discovery — same heuristic as host samples.
// NOT part of the public SurfaceDesc contract; host never supplies a path.
std::wstring FindShaderDirInternal() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
    std::vector<std::filesystem::path> candidates = {
        exeDir / "shaders",
        exeDir / ".." / "shaders",
        exeDir / ".." / ".." / "shaders",
        std::filesystem::current_path() / "shaders",
        std::filesystem::current_path() / ".." / "shaders",
    };
    for (auto& c : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(c / "glass.hlsl", ec))
            return std::filesystem::absolute(c).wstring();
    }
    return L"";
}

// Constant buffer layouts (must match HLSL exactly). Internal detail.
struct FrameCBData {
    float resolution[4];
    float time[4];
};

struct MaterialCBData {
    float m_A[4];
    float m_B[4];
    float m_C[4];
    float m_D[4];
    float m_E[4];
    float m_Stages[4];
    float m_Stages2[4];
};

struct BlurCBData {
    float texel[4];
    float param[4];
};

Status WriteDynamicBuffer(ID3D11DeviceContext* ctx, ID3D11Buffer* buf,
                          const void* data, size_t size) {
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = ctx->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) {
        if (IsDeviceLostHResult(hr)) return Status::DeviceLost(hr);
        return Status::ResourceError(hr);
    }
    memcpy(mapped.pData, data, size);
    ctx->Unmap(buf, 0);
    return Status::Ok();
}

} // anonymous namespace

// ============================================================
// Lifecycle
// ============================================================

GlassSurface::~GlassSurface() {
    Reset();
}

GlassSurface::GlassSurface(GlassSurface&& other) noexcept
    : device_(std::move(other.device_)),
      fsVS_(std::move(other.fsVS_)),
      blurPS_(std::move(other.blurPS_)),
      glassPS_(std::move(other.glassPS_)),
      blurTexA_(std::move(other.blurTexA_)),
      blurRTVA_(std::move(other.blurRTVA_)),
      blurSRVA_(std::move(other.blurSRVA_)),
      blurTexB_(std::move(other.blurTexB_)),
      blurRTVB_(std::move(other.blurRTVB_)),
      blurSRVB_(std::move(other.blurSRVB_)),
      frameCB_(std::move(other.frameCB_)),
      materialCB_(std::move(other.materialCB_)),
      blurCB_(std::move(other.blurCB_)),
      linearSampler_(std::move(other.linearSampler_)),
      shaderDir_(std::move(other.shaderDir_)),
      material_(other.material_),
      width_(other.width_),
      height_(other.height_) {
    other.width_ = other.height_ = 0;
}

GlassSurface& GlassSurface::operator=(GlassSurface&& other) noexcept {
    if (this != &other) {
        Reset();
        device_          = std::move(other.device_);
        fsVS_            = std::move(other.fsVS_);
        blurPS_          = std::move(other.blurPS_);
        glassPS_         = std::move(other.glassPS_);
        blurTexA_        = std::move(other.blurTexA_);
        blurRTVA_        = std::move(other.blurRTVA_);
        blurSRVA_        = std::move(other.blurSRVA_);
        blurTexB_        = std::move(other.blurTexB_);
        blurRTVB_        = std::move(other.blurRTVB_);
        blurSRVB_        = std::move(other.blurSRVB_);
        frameCB_         = std::move(other.frameCB_);
        materialCB_      = std::move(other.materialCB_);
        blurCB_          = std::move(other.blurCB_);
        linearSampler_   = std::move(other.linearSampler_);
        shaderDir_       = std::move(other.shaderDir_);
        material_        = other.material_;
        width_           = other.width_;
        height_          = other.height_;
        other.width_     = 0;
        other.height_    = 0;
    }
    return *this;
}

Status GlassSurface::CheckDeviceLost() const noexcept {
    if (!device_) return Status::NotInitialized();
    HRESULT reason = device_->GetDeviceRemovedReason();
    if (IsDeviceLostHResult(reason)) return Status::DeviceLost(reason);
    if (FAILED(reason)) return Status::DeviceError(reason);
    return Status::Ok();
}

void GlassSurface::Reset() noexcept {
    blurSRVB_.Reset(); blurRTVB_.Reset(); blurTexB_.Reset();
    blurSRVA_.Reset(); blurRTVA_.Reset(); blurTexA_.Reset();
    linearSampler_.Reset();
    blurCB_.Reset(); materialCB_.Reset(); frameCB_.Reset();
    glassPS_.Reset(); blurPS_.Reset(); fsVS_.Reset();
    shaderDir_.clear();
    device_.Reset();
    width_ = height_ = 0;
}

// ============================================================
// Create
// ============================================================

Status GlassSurface::Create(ID3D11Device* device, const SurfaceDesc& desc, GlassSurface& out) {
    if (!device) return Status::InvalidArgument();
    if (desc.width == 0 || desc.height == 0) return Status::InvalidArgument();

    // Reject creating on an already-lost device.
    HRESULT reason = device->GetDeviceRemovedReason();
    if (IsDeviceLostHResult(reason)) return Status::DeviceLost(reason);

    // Internal shader discovery (host never supplies path).
    std::wstring shaderDir = FindShaderDirInternal();
    if (shaderDir.empty()) return Status::ShaderError(E_FAIL);

    out.Reset();
    out.device_    = device;      // ComPtr takes ref
    out.shaderDir_ = std::move(shaderDir);
    out.width_     = desc.width;
    out.height_    = desc.height;

    Status s = out.CreateShadersAndResources();
    if (!s.ok()) { out.Reset(); return s; }

    s = out.CreateBlurTargets();
    if (!s.ok()) { out.Reset(); return s; }

    return Status::Ok();
}

Status GlassSurface::CreateShadersAndResources() {
    ShaderLibrary shaders;
    shaders.Init(shaderDir_);

    auto failShader = [](HRESULT hr) -> Status {
        if (IsDeviceLostHResult(hr)) return Status::DeviceLost(hr);
        return Status::ShaderError(hr);
    };

    auto vsBlob = shaders.Compile(L"fullscreen_triangle.hlsl", "FullscreenVS", "vs_5_0");
    if (!vsBlob) return Status::ShaderError(E_FAIL);
    HRESULT hr = device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                             nullptr, &fsVS_);
    if (FAILED(hr)) return failShader(hr);

    auto blurBlob = shaders.Compile(L"blur.hlsl", "BlurPS", "ps_5_0");
    if (!blurBlob) return Status::ShaderError(E_FAIL);
    hr = device_->CreatePixelShader(blurBlob->GetBufferPointer(), blurBlob->GetBufferSize(),
                                    nullptr, &blurPS_);
    if (FAILED(hr)) return failShader(hr);

    auto glassBlob = shaders.Compile(L"glass.hlsl", "GlassPS", "ps_5_0");
    if (!glassBlob) return Status::ShaderError(E_FAIL);
    hr = device_->CreatePixelShader(glassBlob->GetBufferPointer(), glassBlob->GetBufferSize(),
                                    nullptr, &glassPS_);
    if (FAILED(hr)) return failShader(hr);

    D3D11_BUFFER_DESC cbd{};
    cbd.Usage          = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    cbd.ByteWidth = sizeof(FrameCBData);
    hr = device_->CreateBuffer(&cbd, nullptr, &frameCB_);
    if (FAILED(hr)) return Status::ResourceError(hr);

    cbd.ByteWidth = sizeof(MaterialCBData);
    hr = device_->CreateBuffer(&cbd, nullptr, &materialCB_);
    if (FAILED(hr)) return Status::ResourceError(hr);

    cbd.ByteWidth = sizeof(BlurCBData);
    hr = device_->CreateBuffer(&cbd, nullptr, &blurCB_);
    if (FAILED(hr)) return Status::ResourceError(hr);

    D3D11_SAMPLER_DESC sd{};
    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD   = D3D11_FLOAT32_MAX;
    hr = device_->CreateSamplerState(&sd, &linearSampler_);
    if (FAILED(hr)) return Status::ResourceError(hr);

    return Status::Ok();
}

Status GlassSurface::CreateBlurTargets() {
    blurSRVB_.Reset(); blurRTVB_.Reset(); blurTexB_.Reset();
    blurSRVA_.Reset(); blurRTVA_.Reset(); blurTexA_.Reset();

    D3D11_TEXTURE2D_DESC td{};
    td.Width      = width_;
    td.Height     = height_;
    td.MipLevels  = 1;
    td.ArraySize  = 1;
    td.Format     = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage      = D3D11_USAGE_DEFAULT;
    td.BindFlags  = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    auto fail = [](HRESULT hr) -> Status {
        if (IsDeviceLostHResult(hr)) return Status::DeviceLost(hr);
        return Status::ResourceError(hr);
    };

    HRESULT hr;
    hr = device_->CreateTexture2D(&td, nullptr, &blurTexA_);
    if (FAILED(hr)) return fail(hr);
    hr = device_->CreateRenderTargetView(blurTexA_.Get(), nullptr, &blurRTVA_);
    if (FAILED(hr)) return fail(hr);
    hr = device_->CreateShaderResourceView(blurTexA_.Get(), nullptr, &blurSRVA_);
    if (FAILED(hr)) return fail(hr);

    hr = device_->CreateTexture2D(&td, nullptr, &blurTexB_);
    if (FAILED(hr)) return fail(hr);
    hr = device_->CreateRenderTargetView(blurTexB_.Get(), nullptr, &blurRTVB_);
    if (FAILED(hr)) return fail(hr);
    hr = device_->CreateShaderResourceView(blurTexB_.Get(), nullptr, &blurSRVB_);
    if (FAILED(hr)) return fail(hr);

    return Status::Ok();
}

// ============================================================
// Resize
// ============================================================

Status GlassSurface::Resize(uint32_t width, uint32_t height) {
    if (!device_) return Status::NotInitialized();
    if (width == 0 || height == 0) return Status::InvalidArgument();

    Status dl = CheckDeviceLost();
    if (!dl.ok()) return dl;

    if (width == width_ && height == height_) return Status::Ok();

    width_  = width;
    height_ = height;
    return CreateBlurTargets();
}

// ============================================================
// Render
// ============================================================

Status GlassSurface::Render(ID3D11DeviceContext* ctx,
                            ID3D11RenderTargetView* target,
                            ID3D11ShaderResourceView* background,
                            const FrameInfo& frame) {
    if (!device_ || !fsVS_) return Status::NotInitialized();
    if (!ctx)     return Status::InvalidArgument();
    if (!target)  return Status::InvalidArgument();
    if (!background) return Status::InvalidArgument();
    if (width_ == 0 || height_ == 0) return Status::NotInitialized();

    // Fail fast if the device has already been removed/reset.
    Status dl = CheckDeviceLost();
    if (!dl.ok()) return dl;

    const float w = (float)width_;
    const float h = (float)height_;

    ID3D11ShaderResourceView* blurredSRV = background;

    if (frame.stages.blur) {
        // Horizontal: background -> blurA
        {
            D3D11_VIEWPORT vp{};
            vp.Width = w; vp.Height = h; vp.MaxDepth = 1.0f;
            ctx->OMSetRenderTargets(1, blurRTVA_.GetAddressOf(), nullptr);
            ctx->RSSetViewports(1, &vp);
            ctx->IASetInputLayout(nullptr);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->VSSetShader(fsVS_.Get(), nullptr, 0);
            ctx->PSSetShader(blurPS_.Get(), nullptr, 0);

            BlurCBData cb{};
            cb.texel[0] = 1.0f / w;
            cb.texel[1] = 1.0f / h;
            cb.texel[2] = 1.0f; cb.texel[3] = 0.0f;
            cb.param[0] = material_.GetBlurRadius();
            Status s = WriteDynamicBuffer(ctx, blurCB_.Get(), &cb, sizeof(cb));
            if (!s.ok()) return s;
            ctx->PSSetConstantBuffers(0, 1, blurCB_.GetAddressOf());

            ID3D11ShaderResourceView* srvs[] = { background };
            ctx->PSSetShaderResources(0, 1, srvs);
            ctx->PSSetSamplers(0, 1, linearSampler_.GetAddressOf());
            ctx->Draw(3, 0);

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
            cb.texel[0] = 1.0f / w;
            cb.texel[1] = 1.0f / h;
            cb.texel[2] = 0.0f; cb.texel[3] = 1.0f;
            cb.param[0] = material_.GetBlurRadius();
            Status s = WriteDynamicBuffer(ctx, blurCB_.Get(), &cb, sizeof(cb));
            if (!s.ok()) return s;
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

    // Glass composite -> target
    {
        D3D11_VIEWPORT vp{};
        vp.Width = w; vp.Height = h; vp.MaxDepth = 1.0f;
        ctx->OMSetRenderTargets(1, &target, nullptr);
        ctx->RSSetViewports(1, &vp);
        ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(fsVS_.Get(), nullptr, 0);
        ctx->PSSetShader(glassPS_.Get(), nullptr, 0);

        FrameCBData fcb{};
        fcb.resolution[0] = w;
        fcb.resolution[1] = h;
        fcb.time[0] = frame.timeSeconds;
        Status s = WriteDynamicBuffer(ctx, frameCB_.Get(), &fcb, sizeof(fcb));
        if (!s.ok()) return s;

        float hlX, hlY;
        material_.GetHighlightPosition(hlX, hlY);

        MaterialCBData mcb{};
        mcb.m_A[0] = material_.GetBlurRadius();
        mcb.m_A[1] = material_.GetRefractionStrength();
        mcb.m_A[2] = material_.GetDispersionStrength();
        mcb.m_A[3] = material_.GetThickness();
        mcb.m_B[0] = material_.GetEdgeFresnel();
        mcb.m_B[1] = material_.GetSpecularStrength();
        mcb.m_B[2] = material_.GetTintAmount();
        mcb.m_B[3] = material_.GetSaturation();
        mcb.m_C[0] = material_.GetBrightness();
        mcb.m_C[1] = material_.GetNoiseAmount();
        mcb.m_C[2] = material_.GetCornerRadius();
        mcb.m_C[3] = material_.GetOpacity();
        mcb.m_D[0] = w * 0.5f;
        mcb.m_D[1] = h * 0.5f;
        mcb.m_D[2] = w * 0.30f;
        mcb.m_D[3] = h * 0.30f;
        mcb.m_E[0] = hlX;
        mcb.m_E[1] = hlY;
        mcb.m_Stages[0] = frame.stages.refraction  ? 1.0f : 0.0f;
        mcb.m_Stages[1] = frame.stages.dispersion  ? 1.0f : 0.0f;
        mcb.m_Stages[2] = frame.stages.fresnel     ? 1.0f : 0.0f;
        mcb.m_Stages[3] = frame.stages.specular    ? 1.0f : 0.0f;
        mcb.m_Stages2[0] = frame.stages.mask       ? 1.0f : 0.0f;
        mcb.m_Stages2[1] = frame.stages.colorAdjust ? 1.0f : 0.0f;

        s = WriteDynamicBuffer(ctx, materialCB_.Get(), &mcb, sizeof(mcb));
        if (!s.ok()) return s;

        ID3D11Buffer* cbs[] = { frameCB_.Get(), materialCB_.Get() };
        ctx->PSSetConstantBuffers(0, 2, cbs);

        ID3D11ShaderResourceView* srvs[] = { background, blurredSRV };
        ctx->PSSetShaderResources(0, 2, srvs);
        ctx->PSSetSamplers(0, 1, linearSampler_.GetAddressOf());

        ctx->Draw(3, 0);
    }

    return Status::Ok();
}

} // namespace AuroraGlass
