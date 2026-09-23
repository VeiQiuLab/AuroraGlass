using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace AuroraGlass.Wpf;

public enum WpfHostStatus
{
    Ok = 0,
    InvalidArgument = 1,
    InvalidWindow = 2,
    DifferentWindowAlreadyAttached = 3,
    AttachFailed = 4
}

public sealed class WpfHostAttachment : IDisposable
{
    private IntPtr _nativeHost;
    private Window? _window;
    private bool _disposed;

    public WpfHostAttachment()
    {
        _nativeHost = NativeMethods.HostCreate();

        if (_nativeHost == IntPtr.Zero)
        {
            throw new OutOfMemoryException(
                "Failed to create AuroraGlass WPF host attachment.");
        }
    }

    public bool IsAttached
    {
        get
        {
            if (_disposed || _nativeHost == IntPtr.Zero)
            {
                return false;
            }

            return NativeMethods.HostIsAttached(_nativeHost) != 0;
        }
    }

    public WpfHostStatus LastStatus { get; private set; } =
        WpfHostStatus.Ok;

    public bool Attach(Window window)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        ArgumentNullException.ThrowIfNull(window);

        if (_window is not null &&
            !ReferenceEquals(_window, window))
        {
            LastStatus =
                WpfHostStatus.DifferentWindowAlreadyAttached;

            return false;
        }

        if (_window is null)
        {
            _window = window;
            _window.SourceInitialized += OnSourceInitialized;
            _window.Closed += OnClosed;
        }

        return TryAttachCurrentWindow();
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

        NativeMethods.HostDestroy(_nativeHost);

        _nativeHost = IntPtr.Zero;
        _disposed = true;

        GC.SuppressFinalize(this);
    }

    private bool TryAttachCurrentWindow()
    {
        if (_window is null ||
            _nativeHost == IntPtr.Zero)
        {
            LastStatus = WpfHostStatus.InvalidArgument;
            return false;
        }

        IntPtr hwnd =
            new WindowInteropHelper(_window).Handle;

        if (hwnd == IntPtr.Zero)
        {
            LastStatus = WpfHostStatus.InvalidWindow;
            return false;
        }

        LastStatus =
            NativeMethods.HostAttach(
                _nativeHost,
                hwnd);

        return LastStatus == WpfHostStatus.Ok;
    }

    private void OnSourceInitialized(
        object? sender,
        EventArgs e)
    {
        if (_disposed ||
            _window is null ||
            !ReferenceEquals(sender, _window))
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
            !ReferenceEquals(sender, _window))
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

        NativeMethods.HostDetach(_nativeHost);
        LastStatus = WpfHostStatus.Ok;
    }

    private void UnbindWindow()
    {
        if (_window is null)
        {
            return;
        }

        _window.SourceInitialized -= OnSourceInitialized;
        _window.Closed -= OnClosed;
        _window = null;
    }

    private static class NativeMethods
    {
        private const string NativeLibrary =
            "AuroraGlassWpfInterop.dll";

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
    }
}
