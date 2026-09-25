#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdint>

struct SampleD3D11Host {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    bool Init(HWND hwnd) {
        DXGI_SWAP_CHAIN_DESC d{};
        d.BufferCount = 2;
        d.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        d.OutputWindow = hwnd;
        d.SampleDesc.Count = 1;
        d.Windowed = TRUE;
        d.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        HRESULT hr = Create(d, D3D_DRIVER_TYPE_HARDWARE);
        if (FAILED(hr)) hr = Create(d, D3D_DRIVER_TYPE_WARP);
        if (FAILED(hr)) return false;

        RECT rc{};
        if (!GetClientRect(hwnd, &rc)) return false;

        return SUCCEEDED(Resize(
            static_cast<std::uint32_t>(rc.right - rc.left),
            static_cast<std::uint32_t>(rc.bottom - rc.top)));
    }

    HRESULT Resize(std::uint32_t w, std::uint32_t h) {
        if (!device || !context || !swapChain) return E_UNEXPECTED;
        if (w == 0 || h == 0) return E_INVALIDARG;

        context->ClearState();
        context->Flush();
        rtv.Reset();

        HRESULT hr = swapChain->ResizeBuffers(
            0, w, h, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) return hr;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        hr = swapChain->GetBuffer(
            0,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(backBuffer.GetAddressOf()));
        if (FAILED(hr)) return hr;

        hr = device->CreateRenderTargetView(
            backBuffer.Get(), nullptr, rtv.GetAddressOf());
        if (FAILED(hr)) return hr;

        width = w;
        height = h;

        context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);

        D3D11_VIEWPORT vp{};
        vp.Width = static_cast<float>(w);
        vp.Height = static_cast<float>(h);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context->RSSetViewports(1, &vp);

        return S_OK;
    }

    HRESULT Present() {
        return swapChain ? swapChain->Present(1, 0) : E_UNEXPECTED;
    }

private:
    HRESULT Create(
        DXGI_SWAP_CHAIN_DESC& d,
        D3D_DRIVER_TYPE driverType) {
        swapChain.Reset();
        context.Reset();
        device.Reset();

        return D3D11CreateDeviceAndSwapChain(
            nullptr,
            driverType,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &d,
            swapChain.GetAddressOf(),
            device.GetAddressOf(),
            nullptr,
            context.GetAddressOf());
    }
};
