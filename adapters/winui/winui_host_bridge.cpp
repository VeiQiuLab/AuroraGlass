#include "winui/winui_host_bridge.h"

#include "win32/win32_host_attachment.h"

#include <Windows.h>
#include <new>

using AuroraGlass::Adapters::Win32::Win32HostAttachment;

struct AuroraGlassWinUIHostAttachment {
    Win32HostAttachment attachment{};
};

extern "C" {

AuroraGlassWinUIHostAttachment*
AuroraGlassWinUIHostCreate(void) noexcept
{
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

    HWND hwnd =
        reinterpret_cast<HWND>(hwndValue);

    if (!IsWindow(hwnd)) {
        return AURORAGLASS_WINUI_HOST_INVALID_WINDOW;
    }

    if (host->attachment.IsAttached()) {
        if (host->attachment.Window() == hwnd) {
            return AURORAGLASS_WINUI_HOST_OK;
        }

        return
            AURORAGLASS_WINUI_HOST_DIFFERENT_WINDOW_ALREADY_ATTACHED;
    }

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
    return host != nullptr &&
        host->attachment.IsAttached()
        ? 1
        : 0;
}

}