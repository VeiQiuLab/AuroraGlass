
#pragma once
#include <d3d11.h>
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
    void Resize(uint32_t newWidth, uint32_t newHeight);
    // Presents the swap chain. Returns the HRESULT unmodified (not swallowed);
    // the host can test it with IsDeviceLostHResult() to detect device removal.
    HRESULT Present();
    void Clear(float r, float g, float b, float a);
    // Returns true if the device has been removed or reset.
    bool IsDeviceLost() const;
    // Report live device objects (debug layer) for leak diagnosis on shutdown.
    void ReportLiveObjects();
};

}
