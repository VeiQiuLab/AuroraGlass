#include "winui/winui_host_bridge.h"

#include "win32/win32_host_attachment.h"

#include <Windows.h>
#include <new>

using AuroraGlass::Adapters::Win32::Win32HostAttachment;

struct AuroraGlassWinUIHostAttachment {
    Win32HostAttachment attachment{};

    AuroraGlassWinUIHostMetricsCallback metricsCallback = nullptr;
    void* metricsUserData = nullptr;

    void EmitMetrics(
        AuroraGlass::Adapters::Win32::Win32HostMetrics metrics) noexcept
    {
        if (metricsCallback == nullptr) {
            return;
        }

        const AuroraGlassWinUIHostMetrics snapshot{
            metrics.clientWidth,
            metrics.clientHeight,
            static_cast<uint32_t>(metrics.dpi)
        };

        metricsCallback(
            &snapshot,
            metricsUserData);
    }

    void InstallMetricsCallbacks()
    {
        if (metricsCallback == nullptr) {
            attachment.SetResizeCallback({});
            attachment.SetDpiChangedCallback({});
            return;
        }

        attachment.SetResizeCallback(
            [this](
                AuroraGlass::Adapters::Win32::Win32HostMetrics metrics)
            {
                EmitMetrics(metrics);
            });

        attachment.SetDpiChangedCallback(
            [this](
                AuroraGlass::Adapters::Win32::Win32HostMetrics metrics)
            {
                EmitMetrics(metrics);
            });
    }
};

extern "C" {

AuroraGlassWinUIHostAttachment*
AuroraGlassWinUIHostCreate(void) noexcept {
    return new (std::nothrow) AuroraGlassWinUIHostAttachment{};
}

void AuroraGlassWinUIHostDestroy(
    AuroraGlassWinUIHostAttachment* host) noexcept
{
    delete host;
}

int32_t AuroraGlassWinUIHostAttach(
    AuroraGlassWinUIHostAttachment* host,
    intptr_t hwndValue) noexcept
{
    if (host == nullptr || hwndValue == 0) {
        return AURORAGLASS_WINUI_HOST_INVALID_ARGUMENT;
    }

    HWND hwnd = reinterpret_cast<HWND>(hwndValue);

    if (!IsWindow(hwnd)) {
        return AURORAGLASS_WINUI_HOST_INVALID_WINDOW;
    }

    if (host->attachment.IsAttached()) {
        if (host->attachment.Window() == hwnd) {
            return AURORAGLASS_WINUI_HOST_OK;
        }

        return AURORAGLASS_WINUI_HOST_DIFFERENT_WINDOW_ALREADY_ATTACHED;
    }

    host->InstallMetricsCallbacks();

    if (!host->attachment.Attach(hwnd)) {
        return AURORAGLASS_WINUI_HOST_ATTACH_FAILED;
    }

    return AURORAGLASS_WINUI_HOST_OK;
}

void AuroraGlassWinUIHostDetach(
    AuroraGlassWinUIHostAttachment* host) noexcept
{
    if (host != nullptr) {
        host->attachment.Detach();
    }
}

int32_t AuroraGlassWinUIHostIsAttached(
    const AuroraGlassWinUIHostAttachment* host) noexcept
{
    return host != nullptr && host->attachment.IsAttached() ? 1 : 0;
}

}

extern "C" {

int32_t AuroraGlassWinUIHostGetMetrics(
    const AuroraGlassWinUIHostAttachment* host,
    AuroraGlassWinUIHostMetrics* metrics) noexcept
{
    if (host == nullptr ||
        metrics == nullptr)
    {
        return AURORAGLASS_WINUI_HOST_INVALID_ARGUMENT;
    }

    const AuroraGlass::Adapters::Win32::Win32HostMetrics nativeMetrics =
        host->attachment.Metrics();

    metrics->client_width =
        nativeMetrics.clientWidth;

    metrics->client_height =
        nativeMetrics.clientHeight;

    metrics->dpi =
        static_cast<uint32_t>(
            nativeMetrics.dpi);

    return AURORAGLASS_WINUI_HOST_OK;
}

int32_t AuroraGlassWinUIHostSetMetricsCallback(
    AuroraGlassWinUIHostAttachment* host,
    AuroraGlassWinUIHostMetricsCallback callback,
    void* userData) noexcept
{
    if (host == nullptr) {
        return AURORAGLASS_WINUI_HOST_INVALID_ARGUMENT;
    }

    host->metricsCallback =
        callback;

    host->metricsUserData =
        userData;

    if (host->attachment.IsAttached()) {
        host->InstallMetricsCallbacks();
    }

    return AURORAGLASS_WINUI_HOST_OK;
}

}
