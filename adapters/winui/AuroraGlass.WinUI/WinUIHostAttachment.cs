using Microsoft.UI.Xaml;
using System.Runtime.InteropServices;
using Windows.Foundation;
using WinRT.Interop;

namespace AuroraGlass.WinUI;

public sealed class WinUIHostAttachment : IDisposable
{
    private nint nativeHandle;
    private Window? window;
    private nint windowHandle;

    private readonly NativeMethods.HostMetricsCallback
        metricsCallback;

    public WinUIHostAttachment()
    {
        nativeHandle =
            NativeMethods.AuroraGlassWinUIHostCreate();

        if (nativeHandle == 0)
        {
            throw new OutOfMemoryException(
                "Failed to create AuroraGlass WinUI host attachment.");
        }

        metricsCallback =
            OnNativeMetrics;

        int status =
            NativeMethods.AuroraGlassWinUIHostSetMetricsCallback(
                nativeHandle,
                metricsCallback,
                0);

        if (status != 0)
        {
            NativeMethods.AuroraGlassWinUIHostDestroy(
                nativeHandle);

            nativeHandle =
                0;

            throw new InvalidOperationException(
                "Failed to install AuroraGlass WinUI metrics callback.");
        }
    }

    public event Action<WinUIHostMetrics>?
        MetricsChanged;

    public bool IsAttached =>
        nativeHandle != 0 &&
        NativeMethods.AuroraGlassWinUIHostIsAttached(
            nativeHandle) != 0;

    public nint WindowHandle =>
        windowHandle;

    public WinUIHostMetrics CurrentMetrics
    {
        get
        {
            ThrowIfDisposed();

            int status =
                NativeMethods.AuroraGlassWinUIHostGetMetrics(
                    nativeHandle,
                    out NativeMethods.NativeHostMetrics metrics);

            if (status != 0)
            {
                throw new InvalidOperationException(
                    "Failed to query AuroraGlass WinUI host metrics.");
            }

            return ToManaged(
                metrics);
        }
    }

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

    public Point LogicalToPhysical(
        Point logical)
    {
        double scale =
            CurrentMetrics.LogicalToPhysicalScale;

        return new Point(
            logical.X * scale,
            logical.Y * scale);
    }

    public Rect LogicalToPhysical(
        Rect logical)
    {
        double scale =
            CurrentMetrics.LogicalToPhysicalScale;

        return new Rect(
            logical.X * scale,
            logical.Y * scale,
            logical.Width * scale,
            logical.Height * scale);
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

        NativeMethods.AuroraGlassWinUIHostSetMetricsCallback(
            nativeHandle,
            null,
            0);

        NativeMethods.AuroraGlassWinUIHostDestroy(
            nativeHandle);

        nativeHandle =
            0;

        GC.SuppressFinalize(
            this);
    }

    private void OnWindowClosed(
        object sender,
        WindowEventArgs args)
    {
        Detach();
    }

    private void OnNativeMetrics(
        nint metricsPointer,
        nint userData)
    {
        if (metricsPointer == 0 ||
            nativeHandle == 0)
        {
            return;
        }

        NativeMethods.NativeHostMetrics native =
            Marshal.PtrToStructure<NativeMethods.NativeHostMetrics>(
                metricsPointer);

        MetricsChanged?.Invoke(
            ToManaged(
                native));
    }

    private static WinUIHostMetrics ToManaged(
        NativeMethods.NativeHostMetrics metrics)
    {
        return new WinUIHostMetrics(
            metrics.ClientWidth,
            metrics.ClientHeight,
            metrics.Dpi);
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