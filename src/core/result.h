#pragma once

// ============================================================
// AuroraGlass Core — Minimal unified status model.
//
// All public Core operations that can fail return Status.
// No exceptions. No swallowed HRESULT. No magic bool.
// ============================================================

#include <cstdint>
#include <Windows.h>
#include <dxgi.h>

namespace AuroraGlass {

enum class ErrorCode : int32_t {
    Ok              = 0,
    InvalidArgument = 1,   // Parameter out of valid range, NaN, or Inf
    DeviceError     = 2,   // ID3D11Device operation failed
    ResourceError   = 3,   // Resource creation (buffer/texture/RTV/SRV) failed
    ShaderError     = 4,   // Shader compilation or creation failed
    NotInitialized  = 5,   // Operation called on uninitialized/moved-from object
    DeviceLost      = 6,   // GPU device removed/reset; host must recreate device + surface
};

// True if hr indicates the GPU device was removed or reset.
// These are the HRESULTs a host must react to by recreating its D3D device.
inline bool IsDeviceLostHResult(long hr) noexcept {
    return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET;
}

struct Status {
    ErrorCode code = ErrorCode::Ok;
    HRESULT   hr   = S_OK;

    constexpr bool ok() const noexcept { return code == ErrorCode::Ok; }
    constexpr explicit operator bool() const noexcept { return ok(); }

    static Status Ok() noexcept { return { ErrorCode::Ok, S_OK }; }

    static Status InvalidArgument(HRESULT hr = E_INVALIDARG) noexcept {
        return { ErrorCode::InvalidArgument, hr };
    }
    static Status DeviceError(HRESULT hr) noexcept {
        return { ErrorCode::DeviceError, hr };
    }
    static Status ResourceError(HRESULT hr) noexcept {
        return { ErrorCode::ResourceError, hr };
    }
    static Status ShaderError(HRESULT hr) noexcept {
        return { ErrorCode::ShaderError, hr };
    }
    static Status NotInitialized() noexcept {
        return { ErrorCode::NotInitialized, E_UNEXPECTED };
    }
    static Status DeviceLost(HRESULT hr) noexcept {
        return { ErrorCode::DeviceLost, hr };
    }
};

// Human-readable name for ErrorCode (useful for diagnostics).
constexpr const char* ErrorCodeToString(ErrorCode c) noexcept {
    switch (c) {
    case ErrorCode::Ok:              return "Ok";
    case ErrorCode::InvalidArgument: return "InvalidArgument";
    case ErrorCode::DeviceError:     return "DeviceError";
    case ErrorCode::ResourceError:   return "ResourceError";
    case ErrorCode::ShaderError:     return "ShaderError";
    case ErrorCode::NotInitialized:  return "NotInitialized";
    case ErrorCode::DeviceLost:      return "DeviceLost";
    }
    return "Unknown";
}

} // namespace AuroraGlass
