
#pragma once
#include <d3d11.h>
#include <d3d11sdklayers.h>   // D3D11_RLDO_FLAGS
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdint>

namespace AuroraGlass {

struct D3D11Device {
    Microsoft::WRL::ComPtr<ID3D11Device>        device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGISwapChain>      swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    Microsoft::WRL::ComPtr<ID3D11Debug>         debugDevice;
    uint32_t width = 0;
    uint32_t height = 0;
    bool debugLayerActive = false;

    bool Init(HWND hwnd);
    // Resizes the swap chain + backbuffer RTV. Returns the HRESULT unmodified
    // (not swallowed). On failure the previous size is preserved where possible.
    // Device-lost HRESULTs are returned as-is for the host to classify.
    HRESULT Resize(uint32_t newWidth, uint32_t newHeight);
    // Presents the swap chain. Returns the HRESULT unmodified (not swallowed);
    // the host can test it with IsDeviceLostHResult() to detect device removal.
    HRESULT Present();
    void Clear(float r, float g, float b, float a);
    // Returns true if the device has been removed or reset.
    bool IsDeviceLost() const;
    // Report live device objects (debug layer) for leak diagnosis on shutdown.
    //
    // Default flags: D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL.
    //
    // Rationale: a valid ID3D11Debug must be kept alive to run the report, and
    // that keeps the ID3D11Device alive. The device and its runtime-internal
    // objects (context, device-context state, default blend/depth/rasterizer/
    // sampler states, an internal query, the swap chain, back-buffer textures
    // and the back-buffer RTV) therefore always appear at report time. Every
    // one of them has NO external COM reference (Refcount 0) at that point —
    // they are runtime-internal, not application leaks. IGNORE_INTERNAL filters
    // exactly those, while a real application leak (which still holds an
    // external reference) is still reported. Pass D3D11_RLDO_DETAIL alone to
    // see the full internal detail.
    void ReportLiveObjects(
        D3D11_RLDO_FLAGS flags = D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
};

}
