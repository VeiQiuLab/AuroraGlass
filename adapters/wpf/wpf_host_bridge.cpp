#include "wpf/wpf_host_bridge.h"

#include "win32/win32_host_attachment.h"

#include <Windows.h>
#include <new>

using AuroraGlass::Adapters::Win32::Win32HostAttachment;

struct AuroraGlassWpfHostAttachment {
    Win32HostAttachment attachment{};

    AuroraGlassWpfHostMetricsCallback metricsCallback = nullptr;
    void* metricsUserData = nullptr;

    void EmitMetrics(
        AuroraGlass::Adapters::Win32::Win32HostMetrics metrics) noexcept
    {
        if (metricsCallback == nullptr) {
            return;
        }

        const AuroraGlassWpfHostMetrics snapshot{
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

AuroraGlassWpfHostAttachment*
AuroraGlassWpfHostCreate(void) noexcept {
    return new (std::nothrow) AuroraGlassWpfHostAttachment{};
}

void AuroraGlassWpfHostDestroy(
    AuroraGlassWpfHostAttachment* host) noexcept
{
    delete host;
}

int32_t AuroraGlassWpfHostAttach(
    AuroraGlassWpfHostAttachment* host,
    intptr_t hwndValue) noexcept
{
    if (host == nullptr || hwndValue == 0) {
        return AURORAGLASS_WPF_HOST_INVALID_ARGUMENT;
    }

    HWND hwnd = reinterpret_cast<HWND>(hwndValue);

    if (!IsWindow(hwnd)) {
        return AURORAGLASS_WPF_HOST_INVALID_WINDOW;
    }

    if (host->attachment.IsAttached()) {
        if (host->attachment.Window() == hwnd) {
            return AURORAGLASS_WPF_HOST_OK;
        }

        return AURORAGLASS_WPF_HOST_DIFFERENT_WINDOW_ALREADY_ATTACHED;
    }

    host->InstallMetricsCallbacks();

    if (!host->attachment.Attach(hwnd)) {
        return AURORAGLASS_WPF_HOST_ATTACH_FAILED;
    }

    return AURORAGLASS_WPF_HOST_OK;
}

void AuroraGlassWpfHostDetach(
    AuroraGlassWpfHostAttachment* host) noexcept
{
    if (host != nullptr) {
        host->attachment.Detach();
    }
}

int32_t AuroraGlassWpfHostIsAttached(
    const AuroraGlassWpfHostAttachment* host) noexcept
{
    return host != nullptr && host->attachment.IsAttached() ? 1 : 0;
}

}

extern "C" {

int32_t AuroraGlassWpfHostGetMetrics(
    const AuroraGlassWpfHostAttachment* host,
    AuroraGlassWpfHostMetrics* metrics) noexcept
{
    if (host == nullptr ||
        metrics == nullptr)
    {
        return AURORAGLASS_WPF_HOST_INVALID_ARGUMENT;
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

    return AURORAGLASS_WPF_HOST_OK;
}

int32_t AuroraGlassWpfHostSetMetricsCallback(
    AuroraGlassWpfHostAttachment* host,
    AuroraGlassWpfHostMetricsCallback callback,
    void* userData) noexcept
{
    if (host == nullptr) {
        return AURORAGLASS_WPF_HOST_INVALID_ARGUMENT;
    }

    host->metricsCallback =
        callback;

    host->metricsUserData =
        userData;

    if (host->attachment.IsAttached()) {
        host->InstallMetricsCallbacks();
    }

    return AURORAGLASS_WPF_HOST_OK;
}

}
