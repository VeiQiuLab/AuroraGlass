#pragma once

#include <stdint.h>

#if defined(_WIN32)
    #if defined(AURORAGLASS_WPF_INTEROP_EXPORTS)
        #define AURORAGLASS_WPF_HOST_API __declspec(dllexport)
    #else
        #define AURORAGLASS_WPF_HOST_API __declspec(dllimport)
    #endif
#else
    #define AURORAGLASS_WPF_HOST_API
#endif

#if defined(__cplusplus)
    #define AURORAGLASS_WPF_HOST_NOEXCEPT noexcept
    extern "C" {
#else
    #define AURORAGLASS_WPF_HOST_NOEXCEPT
#endif

typedef struct AuroraGlassWpfHostAttachment AuroraGlassWpfHostAttachment;

typedef enum AuroraGlassWpfHostStatus {
    AURORAGLASS_WPF_HOST_OK = 0,
    AURORAGLASS_WPF_HOST_INVALID_ARGUMENT = 1,
    AURORAGLASS_WPF_HOST_INVALID_WINDOW = 2,
    AURORAGLASS_WPF_HOST_DIFFERENT_WINDOW_ALREADY_ATTACHED = 3,
    AURORAGLASS_WPF_HOST_ATTACH_FAILED = 4
} AuroraGlassWpfHostStatus;

AURORAGLASS_WPF_HOST_API AuroraGlassWpfHostAttachment*
AuroraGlassWpfHostCreate(void) AURORAGLASS_WPF_HOST_NOEXCEPT;

AURORAGLASS_WPF_HOST_API void
AuroraGlassWpfHostDestroy(
    AuroraGlassWpfHostAttachment* host) AURORAGLASS_WPF_HOST_NOEXCEPT;

AURORAGLASS_WPF_HOST_API int32_t
AuroraGlassWpfHostAttach(
    AuroraGlassWpfHostAttachment* host,
    intptr_t hwnd) AURORAGLASS_WPF_HOST_NOEXCEPT;

AURORAGLASS_WPF_HOST_API void
AuroraGlassWpfHostDetach(
    AuroraGlassWpfHostAttachment* host) AURORAGLASS_WPF_HOST_NOEXCEPT;

AURORAGLASS_WPF_HOST_API int32_t
AuroraGlassWpfHostIsAttached(
    const AuroraGlassWpfHostAttachment* host) AURORAGLASS_WPF_HOST_NOEXCEPT;


typedef struct AuroraGlassWpfHostMetrics {
    uint32_t client_width;
    uint32_t client_height;
    uint32_t dpi;
} AuroraGlassWpfHostMetrics;

typedef void (*AuroraGlassWpfHostMetricsCallback)(
    const AuroraGlassWpfHostMetrics* metrics,
    void* user_data);

AURORAGLASS_WPF_HOST_API int32_t
AuroraGlassWpfHostGetMetrics(
    const AuroraGlassWpfHostAttachment* host,
    AuroraGlassWpfHostMetrics* metrics)
    AURORAGLASS_WPF_HOST_NOEXCEPT;

AURORAGLASS_WPF_HOST_API int32_t
AuroraGlassWpfHostSetMetricsCallback(
    AuroraGlassWpfHostAttachment* host,
    AuroraGlassWpfHostMetricsCallback callback,
    void* user_data)
    AURORAGLASS_WPF_HOST_NOEXCEPT;
#if defined(__cplusplus)
    }
#endif

#undef AURORAGLASS_WPF_HOST_NOEXCEPT
