#include "winui/winui_render_host_bridge.h"
#include "wpf/wpf_render_host_bridge.h"

#include <Windows.h>
#include <CommCtrl.h>
#include <memory>
#include <new>

struct AuroraGlassWinUIRenderHost {
    AuroraGlassWpfRenderHost* inner = nullptr;
    HWND child = nullptr;
    bool subclassInstalled = false;
};

namespace {

UINT_PTR SubclassId(
    const AuroraGlassWinUIRenderHost* host) noexcept
{
    return reinterpret_cast<UINT_PTR>(host);
}

LRESULT CALLBACK ChildProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR subclassId,
    DWORD_PTR referenceData) noexcept
{
    auto* host =
        reinterpret_cast<AuroraGlassWinUIRenderHost*>(
            referenceData);

    if (message == WM_NCHITTEST) {
        return HTTRANSPARENT;
    }

    if (message == WM_NCDESTROY &&
        host != nullptr)
    {
        RemoveWindowSubclass(
            hwnd,
            ChildProc,
            subclassId);

        host->subclassInstalled = false;
        host->child = nullptr;
    }

    return DefSubclassProc(
        hwnd,
        message,
        wParam,
        lParam);
}

AuroraGlassWpfMaterialSnapshot MapMaterial(
    const AuroraGlassWinUIMaterialSnapshot& source) noexcept
{
    AuroraGlassWpfMaterialSnapshot target{};

    target.blurRadius = source.blurRadius;
    target.refractionStrength = source.refractionStrength;
    target.dispersionStrength = source.dispersionStrength;
    target.thickness = source.thickness;
    target.edgeFresnel = source.edgeFresnel;
    target.specularStrength = source.specularStrength;
    target.tintAmount = source.tintAmount;
    target.saturation = source.saturation;
    target.brightness = source.brightness;
    target.noiseAmount = source.noiseAmount;
    target.cornerRadius = source.cornerRadius;
    target.opacity = source.opacity;
    target.highlightX = source.highlightX;
    target.highlightY = source.highlightY;

    return target;
}

}

extern "C" {

AuroraGlassWinUIRenderHost*
AuroraGlassWinUIRenderHostCreate(
    intptr_t parentHwnd) noexcept
{
    HWND parent =
        reinterpret_cast<HWND>(
            parentHwnd);

    if (parent == nullptr ||
        !IsWindow(parent))
    {
        return nullptr;
    }

    auto* host =
        new (std::nothrow)
            AuroraGlassWinUIRenderHost{};

    if (host == nullptr) {
        return nullptr;
    }

    host->inner =
        AuroraGlassWpfRenderHostCreate(
            parentHwnd);

    if (host->inner == nullptr) {
        delete host;
        return nullptr;
    }

    host->child =
        reinterpret_cast<HWND>(
            AuroraGlassWpfRenderHostGetHwnd(
                host->inner));

    if (host->child == nullptr ||
        !IsWindow(host->child))
    {
        AuroraGlassWpfRenderHostDestroy(
            host->inner);

        delete host;

        return nullptr;
    }

    if (!SetWindowSubclass(
            host->child,
            ChildProc,
            SubclassId(host),
            reinterpret_cast<DWORD_PTR>(host)))
    {
        AuroraGlassWpfRenderHostDestroy(
            host->inner);

        delete host;

        return nullptr;
    }

    host->subclassInstalled = true;

    return host;
}

void AuroraGlassWinUIRenderHostDestroy(
    AuroraGlassWinUIRenderHost* host) noexcept
{
    if (host == nullptr) {
        return;
    }

    if (host->subclassInstalled &&
        host->child != nullptr &&
        IsWindow(host->child))
    {
        RemoveWindowSubclass(
            host->child,
            ChildProc,
            SubclassId(host));
    }

    AuroraGlassWpfRenderHostDestroy(
        host->inner);

    delete host;
}

intptr_t AuroraGlassWinUIRenderHostGetHwnd(
    const AuroraGlassWinUIRenderHost* host) noexcept
{
    if (host == nullptr ||
        host->inner == nullptr)
    {
        return 0;
    }

    return AuroraGlassWpfRenderHostGetHwnd(
        host->inner);
}

int32_t AuroraGlassWinUIRenderHostSetMaterial(
    AuroraGlassWinUIRenderHost* host,
    const AuroraGlassWinUIMaterialSnapshot* snapshot) noexcept
{
    if (host == nullptr ||
        host->inner == nullptr)
    {
        return -1001;
    }

    if (snapshot == nullptr) {
        return AuroraGlassWpfRenderHostSetMaterial(
            host->inner,
            nullptr);
    }

    const AuroraGlassWpfMaterialSnapshot mapped =
        MapMaterial(
            *snapshot);

    return AuroraGlassWpfRenderHostSetMaterial(
        host->inner,
        &mapped);
}

int32_t AuroraGlassWinUIRenderHostSetRects(
    AuroraGlassWinUIRenderHost* host,
    const AuroraGlassWinUIRenderRect* rects,
    uint32_t count) noexcept
{
    if (host == nullptr ||
        host->inner == nullptr)
    {
        return -1001;
    }

    if (count == 0) {
        return AuroraGlassWpfRenderHostSetRects(
            host->inner,
            nullptr,
            0);
    }

    if (rects == nullptr) {
        return AuroraGlassWpfRenderHostSetRects(
            host->inner,
            nullptr,
            count);
    }

    std::unique_ptr<AuroraGlassWpfRenderRect[]>
        mapped{
            new (std::nothrow)
                AuroraGlassWpfRenderRect[count]
        };

    if (!mapped) {
        return -1004;
    }

    for (uint32_t index = 0;
         index < count;
         ++index)
    {
        mapped[index].x = rects[index].x;
        mapped[index].y = rects[index].y;
        mapped[index].width = rects[index].width;
        mapped[index].height = rects[index].height;
    }

    return AuroraGlassWpfRenderHostSetRects(
        host->inner,
        mapped.get(),
        count);
}

int32_t AuroraGlassWinUIRenderHostGetStats(
    const AuroraGlassWinUIRenderHost* host,
    AuroraGlassWinUIRenderStats* outStats) noexcept
{
    if (host == nullptr ||
        host->inner == nullptr ||
        outStats == nullptr)
    {
        return -1001;
    }

    AuroraGlassWpfRenderStats source{};

    const int32_t status =
        AuroraGlassWpfRenderHostGetStats(
            host->inner,
            &source);

    if (status != 0) {
        return status;
    }

    outStats->frameCount = source.frameCount;
    outStats->width = source.width;
    outStats->height = source.height;
    outStats->lastCoreStatus = source.lastCoreStatus;
    outStats->ready = source.ready;

    return 0;
}

int32_t AuroraGlassWinUIRenderHostResize(
    AuroraGlassWinUIRenderHost* host,
    uint32_t width,
    uint32_t height) noexcept
{
    if (host == nullptr ||
        host->inner == nullptr)
    {
        return -1001;
    }

    HWND child =
        reinterpret_cast<HWND>(
            AuroraGlassWpfRenderHostGetHwnd(
                host->inner));

    if (child == nullptr ||
        !IsWindow(child))
    {
        return -1002;
    }

    if (width == 0 ||
        height == 0)
    {
        ShowWindow(
            child,
            SW_HIDE);

        return 0;
    }

    if (!SetWindowPos(
            child,
            nullptr,
            0,
            0,
            static_cast<int>(width),
            static_cast<int>(height),
            SWP_NOZORDER |
            SWP_NOACTIVATE))
    {
        return -1003;
    }

    ShowWindow(
        child,
        SW_SHOWNA);

    return 0;
}

}