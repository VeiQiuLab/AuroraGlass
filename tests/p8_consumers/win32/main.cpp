#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include "core/glass_surface.h"
#include "win32/win32_host_attachment.h"

using Microsoft::WRL::ComPtr;

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.lpszClassName = L"P8FreshWin32";
    if (!RegisterClassW(&wc)) return 1;

    HWND hwnd = CreateWindowW(
        wc.lpszClassName, L"AuroraGlass P8",
        WS_OVERLAPPEDWINDOW, 100, 100, 640, 360,
        nullptr, nullptr, hi, nullptr);
    if (!hwnd) return 2;

    AuroraGlass::Adapters::Win32::Win32HostAttachment host;
    if (!host.Attach(hwnd)) return 3;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;

    if (FAILED(D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &device, nullptr, &context))) return 4;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = 640;
    td.Height = 360;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> bgTex;
    ComPtr<ID3D11RenderTargetView> bgRTV;
    ComPtr<ID3D11ShaderResourceView> bgSRV;
    ComPtr<ID3D11Texture2D> outTex;
    ComPtr<ID3D11RenderTargetView> outRTV;

    if (FAILED(device->CreateTexture2D(&td, nullptr, &bgTex))) return 5;
    if (FAILED(device->CreateRenderTargetView(bgTex.Get(), nullptr, &bgRTV))) return 6;
    if (FAILED(device->CreateShaderResourceView(bgTex.Get(), nullptr, &bgSRV))) return 7;
    if (FAILED(device->CreateTexture2D(&td, nullptr, &outTex))) return 8;
    if (FAILED(device->CreateRenderTargetView(outTex.Get(), nullptr, &outRTV))) return 9;

    float bg[4] = { 0.10f, 0.18f, 0.28f, 1.0f };
    context->ClearRenderTargetView(bgRTV.Get(), bg);

    AuroraGlass::GlassSurface surface;
    AuroraGlass::SurfaceDesc desc{};
    desc.width = 640;
    desc.height = 360;

    if (!AuroraGlass::GlassSurface::Create(device.Get(), desc, surface).ok()) return 10;

    AuroraGlass::GlassMaterial material;
    surface.SetMaterial(material);

    AuroraGlass::FrameInfo frame{};
    if (!surface.Render(context.Get(), outRTV.Get(), bgSRV.Get(), frame).ok()) return 11;

    surface.Reset();
    host.Detach();

    if (host.IsAttached()) return 12;

    DestroyWindow(hwnd);
    return 0;
}