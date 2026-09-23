#pragma once

// ============================================================
// AuroraGlass P7 WinUI 3 Adapter - narrow native host boundary.
//
// WinUI 3 owns the HWND.
// AuroraGlass observes the HWND through the frozen P5
// Win32HostAttachment production lifecycle.
//
// This boundary intentionally does not provide:
//   - window ownership
//   - window management
//   - a second material model
//   - control semantics
//   - rendering semantics
//   - navigation / DI / MVVM infrastructure
// ============================================================

#include <stdint.h>

#if defined(_WIN32)
    #if defined(AURORAGLASS_WINUI_INTEROP_EXPORTS)
        #define AURORAGLASS_WINUI_API __declspec(dllexport)
    #else
        #define AURORAGLASS_WINUI_API __declspec(dllimport)
    #endif
#else
    #define AURORAGLASS_WINUI_API
#endif

#if defined(__cplusplus)
    #define AURORAGLASS_WINUI_NOEXCEPT noexcept
    extern "C" {
#else
    #define AURORAGLASS_WINUI_NOEXCEPT
#endif

typedef struct AuroraGlassWinUIHostAttachment
    AuroraGlassWinUIHostAttachment;

typedef enum AuroraGlassWinUIHostStatus {
    AURORAGLASS_WINUI_HOST_OK = 0,
    AURORAGLASS_WINUI_HOST_INVALID_ARGUMENT = 1,
    AURORAGLASS_WINUI_HOST_INVALID_WINDOW = 2,
    AURORAGLASS_WINUI_HOST_DIFFERENT_WINDOW_ALREADY_ATTACHED = 3,
    AURORAGLASS_WINUI_HOST_ATTACH_FAILED = 4
} AuroraGlassWinUIHostStatus;

AURORAGLASS_WINUI_API
AuroraGlassWinUIHostAttachment*
AuroraGlassWinUIHostCreate(void)
    AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API
void
AuroraGlassWinUIHostDestroy(
    AuroraGlassWinUIHostAttachment* host)
    AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API
int32_t
AuroraGlassWinUIHostAttach(
    AuroraGlassWinUIHostAttachment* host,
    intptr_t hwnd)
    AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API
void
AuroraGlassWinUIHostDetach(
    AuroraGlassWinUIHostAttachment* host)
    AURORAGLASS_WINUI_NOEXCEPT;

AURORAGLASS_WINUI_API
int32_t
AuroraGlassWinUIHostIsAttached(
    const AuroraGlassWinUIHostAttachment* host)
    AURORAGLASS_WINUI_NOEXCEPT;

#if defined(__cplusplus)
    }
#endif

#undef AURORAGLASS_WINUI_NOEXCEPT