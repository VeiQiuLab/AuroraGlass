using Microsoft.UI.Xaml;
using WinRT.Interop;

namespace AuroraGlass.WinUI;

public sealed class WinUIHostAttachment : IDisposable
{
    private nint nativeHandle;
    private Window? window;
    private nint windowHandle;

    public WinUIHostAttachment()
    {
        nativeHandle =
            NativeMethods.AuroraGlassWinUIHostCreate();

        if (nativeHandle == 0)
        {
            throw new OutOfMemoryException(
                "Failed to create AuroraGlass WinUI host attachment.");
        }
    }

    public bool IsAttached =>
        nativeHandle != 0 &&
        NativeMethods.AuroraGlassWinUIHostIsAttached(
            nativeHandle) != 0;

    public nint WindowHandle =>
        windowHandle;

    public void Attach(
        Window targetWindow)
    {
        ArgumentNullException.ThrowIfNull(
            targetWindow);

        ThrowIfDisposed();

        nint hwnd =
            WindowNative.GetWindowHandle(
                targetWindow);

        if (hwnd == 0)
        {
            throw new InvalidOperationException(
                "The WinUI 3 Window does not have a live HWND.");
        }

        int status =
            NativeMethods.AuroraGlassWinUIHostAttach(
                nativeHandle,
                hwnd);

        if (status != 0)
        {
            throw new InvalidOperationException(
                "AuroraGlass native host attach failed with status " +
                status +
                ".");
        }

        if (!ReferenceEquals(
                window,
                targetWindow))
        {
            if (window is not null)
            {
                window.Closed -=
                    OnWindowClosed;
            }

            window =
                targetWindow;

            window.Closed +=
                OnWindowClosed;
        }

        windowHandle =
            hwnd;
    }

    public void Detach()
    {
        if (nativeHandle == 0)
        {
            return;
        }

        if (window is not null)
        {
            window.Closed -=
                OnWindowClosed;

            window =
                null;
        }

        NativeMethods.AuroraGlassWinUIHostDetach(
            nativeHandle);

        windowHandle =
            0;
    }

    public void Dispose()
    {
        if (nativeHandle == 0)
        {
            return;
        }

        Detach();

        NativeMethods.AuroraGlassWinUIHostDestroy(
            nativeHandle);

        nativeHandle =
            0;

        GC.SuppressFinalize(this);
    }

    private void OnWindowClosed(
        object sender,
        WindowEventArgs args)
    {
        Detach();
    }

    private void ThrowIfDisposed()
    {
        if (nativeHandle == 0)
        {
            throw new ObjectDisposedException(
                nameof(WinUIHostAttachment));
        }
    }
}