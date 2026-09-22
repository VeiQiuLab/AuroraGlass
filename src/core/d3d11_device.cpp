
#include "core/d3d11_device.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using namespace Microsoft::WRL;

namespace AuroraGlass {

bool D3D11Device::Init(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 0;
    sd.BufferDesc.RefreshRate.Denominator = 0;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = E_FAIL;

#ifdef _DEBUG
    // Prefer hardware + debug layer when available.
    hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_DEBUG,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &swapChain, &device, &featureLevel, &context);
    if (SUCCEEDED(hr)) debugLayerActive = true;
#endif

    if (FAILED(hr)) {
        // Hardware without debug layer.
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            0, nullptr, 0, D3D11_SDK_VERSION,
            &sd, &swapChain, &device, &featureLevel, &context);
    }
    if (FAILED(hr)) {
        // WARP software fallback.
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            0, nullptr, 0, D3D11_SDK_VERSION,
            &sd, &swapChain, &device, &featureLevel, &context);
    }
    if (FAILED(hr)) return false;

    if (debugLayerActive && device) {
        device->QueryInterface(__uuidof(ID3D11Debug),
                               reinterpret_cast<void**>(debugDevice.GetAddressOf()));
    }

    RECT rc;
    GetClientRect(hwnd, &rc);
    width = rc.right - rc.left;
    height = rc.bottom - rc.top;

    Resize(width, height);
    return true;
}

void D3D11Device::Resize(uint32_t newWidth, uint32_t newHeight) {
    if (!swapChain || !context) return;
    if (newWidth == 0 || newHeight == 0) return;

    context->ClearState();
    context->Flush();

    rtv.Reset();
    HRESULT hr = swapChain->ResizeBuffers(0, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return;

    width = newWidth;
    height = newHeight;

    ComPtr<ID3D11Texture2D> backBuffer;
    hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
    if (FAILED(hr)) return;

    hr = device->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv);
    if (FAILED(hr)) return;

    context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);

    D3D11_VIEWPORT vp;
    vp.Width = (float)newWidth;
    vp.Height = (float)newHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    context->RSSetViewports(1, &vp);
}

HRESULT D3D11Device::Present() {
    if (!swapChain) return E_UNEXPECTED;
    HRESULT hr = swapChain->Present(1, 0);
    // Present can surface device removal via its own HRESULT, or via the
    // device's removed reason when the swap chain itself is already invalid.
    if (FAILED(hr)) return hr;
    if (device) {
        HRESULT reason = device->GetDeviceRemovedReason();
        if (FAILED(reason)) return reason;
    }
    return hr;
}

bool D3D11Device::IsDeviceLost() const {
    if (!device) return false;
    return FAILED(device->GetDeviceRemovedReason());
}

void D3D11Device::Clear(float r, float g, float b, float a) {
    if (rtv) {
        float clearColor[4] = { r, g, b, a };
        context->ClearRenderTargetView(rtv.Get(), clearColor);
    }
}

void D3D11Device::ReportLiveObjects() {
    if (debugDevice) {
        debugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
    }
}

}
