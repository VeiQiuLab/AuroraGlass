#pragma once

// ============================================================
// AuroraGlass Core — GlassSurface (move-only resource-owning type).
//
// GlassSurface owns the GPU resources required to render one
// liquid-glass surface: shaders, constant buffers, intermediate
// render targets, sampler, and a reference to the device.
//
// Ownership rules:
//   • ID3D11Device: HELD via ComPtr (GlassSurface extends its lifetime).
//                   The host must NOT destroy the device while any live
//                   GlassSurface exists that references it.
//   • ID3D11DeviceContext: BORROWED for the duration of Render() only.
//   • ID3D11RenderTargetView (backbuffer): BORROWED during Render() only.
//   • ID3D11ShaderResourceView (background): BORROWED during Render() only.
//   • GlassSurface is NOT copyable. Move construction / move assignment allowed.
//
// Thread contract:
//   All GlassSurface methods (Create / SetMaterial / Resize / Render / Reset /
//   destructor) MUST be called from the same D3D11 render thread that owns
//   the borrowed ID3D11DeviceContext used in Render(). No internal concurrency
//   model is provided by Core v1.
//
// Destruction is deterministic: destructor (or Reset()) frees all GPU resources.
//
// Device-lost contract (P1 minimal):
//   AuroraGlass Core does NOT create or own the system D3D11 device; the host
//   does. Core only detects device-lost/reset and reports it.
//   • CheckDeviceLost() returns Status::DeviceLost if the device that owns this
//     surface has been removed/reset.
//   • Render() / Resize() return Status::DeviceLost if a D3D operation fails
//     because the device was removed/reset (HRESULT is preserved).
//   • On device loss the caller should: Reset() the surface (frees invalid GPU
//     resources), have the host create a new ID3D11Device, then Create() again.
//   • Core does not auto-recreate the device, run background recovery threads,
//     or retry internally.
//
// SetMaterial failure semantics:
//   SetMaterial performs a pure CPU value copy of the GlassMaterial (which
//   itself owns no GPU resources). It cannot fail and returns void.
//   Material validation is performed inside GlassMaterial's setters, not
//   at SetMaterial time.
// ============================================================

#include "core/result.h"
#include "core/glass_material.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>

namespace AuroraGlass {

// Description used to create a GlassSurface.
//
// Only surface geometry is exposed. Shader discovery and compilation
// are internal Core implementation details; the host must not configure
// shader paths, shader layouts, or shader pipeline internals.
// (P1 v1 still loads HLSL from disk at runtime; the discovery path is
// internal and subject to change without public API impact.)
struct SurfaceDesc {
    uint32_t width  = 0;   // initial surface size in pixels
    uint32_t height = 0;
};

// Per-frame information passed to Render().
struct FrameInfo {
    float              timeSeconds = 0.0f;
    DiagnosticStages   stages;         // diagnostic stage toggles (not part of material)
};

class GlassSurface {
public:
    GlassSurface() noexcept = default;
    ~GlassSurface();

    // Non-copyable.
    GlassSurface(const GlassSurface&) = delete;
    GlassSurface& operator=(const GlassSurface&) = delete;

    // Movable.
    GlassSurface(GlassSurface&& other) noexcept;
    GlassSurface& operator=(GlassSurface&& other) noexcept;

    // Factory. device is held via ComPtr (lifetime extended).
    // Returns Status::InvalidArgument if device is null or desc is invalid.
    // Returns Status::ShaderError if any shader fails to compile.
    // Returns Status::ResourceError if any GPU resource creation fails.
    static Status Create(ID3D11Device* device, const SurfaceDesc& desc, GlassSurface& out);

    // Update material. CPU only; next Render picks it up.
    // Never fails for a well-formed GlassMaterial (already validated).
    void SetMaterial(const GlassMaterial& material) noexcept { material_ = material; }

    // Resize internal GPU resources. Returns ResourceError on failure.
    // After successful Resize, next Render uses the new size.
    Status Resize(uint32_t width, uint32_t height);

    // Render one frame.
    // context, target, background are BORROWED for this call only.
    // Returns InvalidArgument if any borrowed pointer is null (when required).
    Status Render(ID3D11DeviceContext* context,
                  ID3D11RenderTargetView* target,
                  ID3D11ShaderResourceView* background,
                  const FrameInfo& frame);

    // Returns Status::DeviceLost if the underlying device has been removed or
    // reset (DXGI_ERROR_DEVICE_REMOVED / DXGI_ERROR_DEVICE_RESET).
    // Returns Status::NotInitialized if no device is held.
    // Returns Status::Ok otherwise.
    Status CheckDeviceLost() const noexcept;

    // Reset the surface to an empty/uninitialized state.
    // Deterministically frees all GPU resources owned by this surface.
    // Idempotent; safe to call multiple times.
    //
    // Normal use does NOT require calling Reset(): the destructor performs
    // deterministic cleanup automatically (RAII). Reset() exists only for
    // explicit re-initialization scenarios.
    //
    // Named Reset() (not Release()) to avoid confusion with IUnknown::Release
    // reference-counting semantics in D3D/COM.
    void Reset() noexcept;

    // Accessors.
    uint32_t Width()  const noexcept { return width_; }
    uint32_t Height() const noexcept { return height_; }
    bool IsInitialized() const noexcept { return device_ != nullptr; }

private:
    // Internal implementation — reused from P0 verified pipeline.
    Status CreateShadersAndResources();
    Status CreateBlurTargets();

    // Device (lifetime extended via ComPtr).
    Microsoft::WRL::ComPtr<ID3D11Device> device_;

    // Shaders (shared fullscreen VS + blur PS + glass PS).
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fsVS_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  blurPS_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  glassPS_;

    // Blur ping-pong intermediate targets.
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          blurTexA_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   blurRTVA_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> blurSRVA_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          blurTexB_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   blurRTVB_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> blurSRVB_;

    // Constant buffers (FrameCB, MaterialCB, BlurCB).
    Microsoft::WRL::ComPtr<ID3D11Buffer> frameCB_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> materialCB_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> blurCB_;

    // Linear clamp sampler.
    Microsoft::WRL::ComPtr<ID3D11SamplerState> linearSampler_;

    // INTERNAL — shader directory discovered by Core at Create time.
    // NOT part of the public API; host never sets or reads this.
    std::wstring shaderDir_;

    // Material (value type; CPU side).
    GlassMaterial material_;

    // Current surface size.
    uint32_t width_  = 0;
    uint32_t height_ = 0;
};

} // namespace AuroraGlass
