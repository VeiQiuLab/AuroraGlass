using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;

namespace AuroraGlass.Wpf;

public enum WpfHostStatus
{
    Ok = 0,
    InvalidArgument = 1,
    InvalidWindow = 2,
    DifferentWindowAlreadyAttached = 3,
    AttachFailed = 4
}

public readonly record struct WpfHostMetrics(
    uint ClientWidth,
    uint ClientHeight,
    uint Dpi)
{
    public double Scale =>
        Dpi == 0
            ? 0.0
            : Dpi / 96.0;

    public Size ClientSizeDip =>
        Scale > 0.0
            ? new Size(
                ClientWidth / Scale,
                ClientHeight / Scale)
            : default;
}

public sealed class WpfHostAttachment : IDisposable
{
    private IntPtr _nativeHost;
    private Window? _window;
    private bool _disposed;

    private readonly NativeMethods.HostMetricsCallback
        _metricsCallback;

    public WpfHostAttachment()
    {
        _nativeHost =
            NativeMethods.HostCreate();

        if (_nativeHost == IntPtr.Zero)
        {
            throw new OutOfMemoryException(
                "Failed to create AuroraGlass WPF host attachment.");
        }

        _metricsCallback =
            OnNativeMetricsChanged;

        WpfHostStatus status =
            NativeMethods.HostSetMetricsCallback(
                _nativeHost,
                _metricsCallback,
                IntPtr.Zero);

        if (status != WpfHostStatus.Ok)
        {
            NativeMethods.HostDestroy(
                _nativeHost);

            _nativeHost =
                IntPtr.Zero;

            throw new InvalidOperationException(
                "Failed to install AuroraGlass host metrics callback.");
        }
    }

    public event Action<WpfHostMetrics>?
        MetricsChanged;

    public bool IsAttached
    {
        get
        {
            if (_disposed ||
                _nativeHost == IntPtr.Zero)
            {
                return false;
            }

            return NativeMethods.HostIsAttached(
                _nativeHost) != 0;
        }
    }

    public WpfHostStatus LastStatus
    {
        get;
        private set;
    } = WpfHostStatus.Ok;

    public WpfHostMetrics CurrentMetrics
    {
        get
        {
            if (_disposed ||
                _nativeHost == IntPtr.Zero)
            {
                return default;
            }

            WpfHostStatus status =
                NativeMethods.HostGetMetrics(
                    _nativeHost,
                    out NativeHostMetrics native);

            if (status != WpfHostStatus.Ok)
            {
                return default;
            }

            return new WpfHostMetrics(
                native.ClientWidth,
                native.ClientHeight,
                native.Dpi);
        }
    }

    internal IntPtr Hwnd
    {
        get
        {
            if (_disposed ||
                _window is null)
            {
                return IntPtr.Zero;
            }

            return new WindowInteropHelper(
                _window).Handle;
        }
    }

    public bool Attach(
        Window window)
    {
        ObjectDisposedException.ThrowIf(
            _disposed,
            this);

        ArgumentNullException.ThrowIfNull(
            window);

        if (_window is not null &&
            !ReferenceEquals(
                _window,
                window))
        {
            LastStatus =
                WpfHostStatus
                    .DifferentWindowAlreadyAttached;

            return false;
        }

        if (_window is null)
        {
            _window =
                window;

            _window.SourceInitialized +=
                OnSourceInitialized;

            _window.Closed +=
                OnClosed;
        }

        return TryAttachCurrentWindow();
    }

    public Point DipToPhysical(
        Point point)
    {
        ObjectDisposedException.ThrowIf(
            _disposed,
            this);

        double scale =
            ResolveConversionScale();

        return new Point(
            point.X * scale,
            point.Y * scale);
    }

    public Rect DipToPhysical(
        Rect rect)
    {
        ObjectDisposedException.ThrowIf(
            _disposed,
            this);

        double scale =
            ResolveConversionScale();

        return new Rect(
            rect.X * scale,
            rect.Y * scale,
            rect.Width * scale,
            rect.Height * scale);
    }

    public void Detach()
    {
        if (_disposed)
        {
            return;
        }

        DetachNative();
        UnbindWindow();
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        DetachNative();
        UnbindWindow();

        NativeMethods.HostSetMetricsCallback(
            _nativeHost,
            null,
            IntPtr.Zero);

        NativeMethods.HostDestroy(
            _nativeHost);

        _nativeHost =
            IntPtr.Zero;

        _disposed =
            true;

        GC.SuppressFinalize(
            this);
    }

    private bool TryAttachCurrentWindow()
    {
        if (_window is null ||
            _nativeHost == IntPtr.Zero)
        {
            LastStatus =
                WpfHostStatus.InvalidArgument;

            return false;
        }

        IntPtr hwnd =
            new WindowInteropHelper(
                _window).Handle;

        if (hwnd == IntPtr.Zero)
        {
            LastStatus =
                WpfHostStatus.InvalidWindow;

            return false;
        }

        LastStatus =
            NativeMethods.HostAttach(
                _nativeHost,
                hwnd);

        return LastStatus ==
            WpfHostStatus.Ok;
    }

    private double ResolveConversionScale()
    {
        WpfHostMetrics metrics =
            CurrentMetrics;

        if (metrics.Dpi > 0)
        {
            return metrics.Dpi / 96.0;
        }

        if (_window is not null)
        {
            DpiScale wpfDpi =
                VisualTreeHelper.GetDpi(
                    _window);

            if (wpfDpi.PixelsPerDip > 0.0)
            {
                return wpfDpi.PixelsPerDip;
            }
        }

        return 1.0;
    }

    private void OnNativeMetricsChanged(
        ref NativeHostMetrics native,
        IntPtr userData)
    {
        MetricsChanged?.Invoke(
            new WpfHostMetrics(
                native.ClientWidth,
                native.ClientHeight,
                native.Dpi));
    }

    private void OnSourceInitialized(
        object? sender,
        EventArgs e)
    {
        if (_disposed ||
            _window is null ||
            !ReferenceEquals(
                sender,
                _window))
        {
            return;
        }

        TryAttachCurrentWindow();
    }

    private void OnClosed(
        object? sender,
        EventArgs e)
    {
        if (_disposed ||
            _window is null ||
            !ReferenceEquals(
                sender,
                _window))
        {
            return;
        }

        DetachNative();
        UnbindWindow();
    }

    private void DetachNative()
    {
        if (_nativeHost == IntPtr.Zero)
        {
            return;
        }

        NativeMethods.HostDetach(
            _nativeHost);

        LastStatus =
            WpfHostStatus.Ok;
    }

    private void UnbindWindow()
    {
        if (_window is null)
        {
            return;
        }

        _window.SourceInitialized -=
            OnSourceInitialized;

        _window.Closed -=
            OnClosed;

        _window =
            null;
    }

    [StructLayout(
        LayoutKind.Sequential)]
    private struct NativeHostMetrics
    {
        public uint ClientWidth;
        public uint ClientHeight;
        public uint Dpi;
    }

    private static class NativeMethods
    {
        private const string NativeLibrary =
            "AuroraGlassWpfInterop.dll";

        [UnmanagedFunctionPointer(
            CallingConvention.Cdecl)]
        internal delegate void HostMetricsCallback(
            ref NativeHostMetrics metrics,
            IntPtr userData);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfHostCreate",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr HostCreate();

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfHostDestroy",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern void HostDestroy(
            IntPtr host);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfHostAttach",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WpfHostStatus HostAttach(
            IntPtr host,
            IntPtr hwnd);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfHostDetach",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern void HostDetach(
            IntPtr host);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfHostIsAttached",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int HostIsAttached(
            IntPtr host);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfHostGetMetrics",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WpfHostStatus HostGetMetrics(
            IntPtr host,
            out NativeHostMetrics metrics);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfHostSetMetricsCallback",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WpfHostStatus
            HostSetMetricsCallback(
                IntPtr host,
                HostMetricsCallback? callback,
                IntPtr userData);
    }
}
