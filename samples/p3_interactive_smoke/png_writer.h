#pragma once
// ============================================================
// AuroraGlass P3 interactive sample - SAMPLE-ONLY PNG writer (WIC).
// Minimal, no external image framework. Not part of Core.
// ============================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <string>
#include <vector>

namespace sample {

// Writes RGBA8 (top-down) pixels to a PNG file via WIC.
inline bool WritePng(const std::wstring& path, const uint8_t* rgba,
                     UINT width, UINT height) {
    using Microsoft::WRL::ComPtr;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool uninit = SUCCEEDED(hr);
    if (hr == RPC_E_CHANGED_MODE) uninit = false;   // already inited as MTA

    ComPtr<IWICImagingFactory> factory;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { if (uninit) CoUninitialize(); return false; }

    ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(&stream))) { if (uninit) CoUninitialize(); return false; }
    if (FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE))) {
        if (uninit) CoUninitialize(); return false;
    }

    ComPtr<IWICBitmapEncoder> encoder;
    if (FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder))) {
        if (uninit) CoUninitialize(); return false;
    }
    if (FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) {
        if (uninit) CoUninitialize(); return false;
    }

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    if (FAILED(encoder->CreateNewFrame(&frame, &props))) { if (uninit) CoUninitialize(); return false; }
    if (FAILED(frame->Initialize(props.Get()))) { if (uninit) CoUninitialize(); return false; }
    if (FAILED(frame->SetSize(width, height))) { if (uninit) CoUninitialize(); return false; }

    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppRGBA;
    if (FAILED(frame->SetPixelFormat(&fmt))) { if (uninit) CoUninitialize(); return false; }

    const UINT stride = width * 4;
    const UINT bufSize = stride * height;
    if (FAILED(frame->WritePixels(height, stride, bufSize, (BYTE*)rgba))) {
        if (uninit) CoUninitialize(); return false;
    }
    bool ok = SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
    if (uninit) CoUninitialize();
    return ok;
}

} // namespace sample
