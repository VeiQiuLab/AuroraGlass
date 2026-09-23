#pragma once

#include <stdint.h>

#if defined(_WIN32)
    #if defined(AURORAGLASS_WINUI_INTEROP_EXPORTS)
        #define AURORAGLASS_WINUI_HOST_API __declspec(dllexport)
    #else
        #define AURORAGLASS_WINUI_HOST_API __declspec(dllimport)
    #endif
#else
    #define AURORAGLASS_WINUI_HOST_API
#endif

#if defined(__cplusplus)
    #define AURORAGLASS_WINUI_HOST_NOEXCEPT noexcept
    extern "C" {
#else
    #define AURORAGLASS_WINUI_HOST_NOEXCEPT
#endif

typedef struct AuroraGlassWinUIHostAttachment AuroraGlassWinUIHostAttachment;

typedef enum AuroraGlassWinUIHostStatus {
    AURORAGLASS_WINUI_HOST_OK = 0,
    AURORAGLASS_WINUI_HOST_INVALID_ARGUMENT = 1,
    AURORAGLASS_WINUI_HOST_INVALID_WINDOW = 2,
    AURORAGLASS_WINUI_HOST_DIFFERENT_WINDOW_ALREADY_ATTACHED = 3,
    AURORAGLASS_WINUI_HOST_ATTACH_FAILED = 4
} AuroraGlassWinUIHostStatus;

AURORAGLASS_WINUI_HOST_API AuroraGlassWinUIHostAttachment*
AuroraGlassWinUIHostCreate(void) AURORAGLASS_WINUI_HOST_NOEXCEPT;

AURORAGLASS_WINUI_HOST_API void
AuroraGlassWinUIHostDestroy(
    AuroraGlassWinUIHostAttachment* host) AURORAGLASS_WINUI_HOST_NOEXCEPT;

AURORAGLASS_WINUI_HOST_API int32_t
AuroraGlassWinUIHostAttach(
    AuroraGlassWinUIHostAttachment* host,
    intptr_t hwnd) AURORAGLASS_WINUI_HOST_NOEXCEPT;

AURORAGLASS_WINUI_HOST_API void
AuroraGlassWinUIHostDetach(
    AuroraGlassWinUIHostAttachment* host) AURORAGLASS_WINUI_HOST_NOEXCEPT;

AURORAGLASS_WINUI_HOST_API int32_t
AuroraGlassWinUIHostIsAttached(
    const AuroraGlassWinUIHostAttachment* host) AURORAGLASS_WINUI_HOST_NOEXCEPT;


typedef struct AuroraGlassWinUIHostMetrics {
    uint32_t client_width;
    uint32_t client_height;
    uint32_t dpi;
} AuroraGlassWinUIHostMetrics;

typedef void (*AuroraGlassWinUIHostMetricsCallback)(
    const AuroraGlassWinUIHostMetrics* metrics,
    void* user_data);

AURORAGLASS_WINUI_HOST_API int32_t
AuroraGlassWinUIHostGetMetrics(
    const AuroraGlassWinUIHostAttachment* host,
    AuroraGlassWinUIHostMetrics* metrics)
    AURORAGLASS_WINUI_HOST_NOEXCEPT;

AURORAGLASS_WINUI_HOST_API int32_t
AuroraGlassWinUIHostSetMetricsCallback(
    AuroraGlassWinUIHostAttachment* host,
    AuroraGlassWinUIHostMetricsCallback callback,
    void* user_data)
    AURORAGLASS_WINUI_HOST_NOEXCEPT;
#if defined(__cplusplus)
    }
#endif

#undef AURORAGLASS_WINUI_HOST_NOEXCEPT
