#include "wpf/wpf_host_bridge.h"

#include "win32/win32_host_attachment.h"

#include <Windows.h>
#include <new>

using AuroraGlass::Adapters::Win32::Win32HostAttachment;

struct AuroraGlassWpfHostAttachment {
    Win32HostAttachment attachment{};
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
