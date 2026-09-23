using System.Runtime.InteropServices;
using System.Windows;

namespace AuroraGlass.Wpf;

public enum WpfInputStatus
{
    Ok = 0,
    InvalidArgument = 1,
    InvalidWindow = 2,
    DifferentWindowAlreadyAttached = 3,
    AttachFailed = 4,
    AddControlFailed = 5
}

public sealed class WpfControlInputBridge : IDisposable
{
    private IntPtr _nativeBridge;
    private WpfHostAttachment? _host;
    private bool _disposed;

    public WpfControlInputBridge()
    {
        _nativeBridge =
            NativeMethods.InputCreate();

        if (_nativeBridge == IntPtr.Zero)
        {
            throw new OutOfMemoryException(
                "Failed to create AuroraGlass WPF input bridge.");
        }
    }

    public bool IsAttached =>
        !_disposed &&
        _nativeBridge != IntPtr.Zero &&
        NativeMethods.InputIsAttached(
            _nativeBridge) != 0;

    public bool OwnsCapture =>
        !_disposed &&
        _nativeBridge != IntPtr.Zero &&
        NativeMethods.InputOwnsCapture(
            _nativeBridge) != 0;

    public WpfInputStatus Attach(
        WpfHostAttachment host)
    {
        ObjectDisposedException.ThrowIf(
            _disposed,
            this);

        ArgumentNullException.ThrowIfNull(
            host);

        if (!host.IsAttached ||
            host.Hwnd == IntPtr.Zero)
        {
            return WpfInputStatus.InvalidWindow;
        }

        WpfInputStatus status =
            NativeMethods.InputAttach(
                _nativeBridge,
                host.Hwnd);

        if (status == WpfInputStatus.Ok)
        {
            _host =
                host;
        }

        return status;
    }

    public WpfButton AddButton(
        Rect boundsDip)
    {
        EnsureAttached();

        Rect physical =
            _host!.DipToPhysical(
                boundsDip);

        WpfInputStatus status =
            NativeMethods.InputAddButton(
                _nativeBridge,
                (float)physical.X,
                (float)physical.Y,
                (float)physical.Width,
                (float)physical.Height,
                out IntPtr handle);

        ThrowIfFailed(
            status,
            "button");

        return new WpfButton(
            this,
            handle);
    }

    public WpfToggle AddToggle(
        Rect boundsDip)
    {
        EnsureAttached();

        Rect physical =
            _host!.DipToPhysical(
                boundsDip);

        WpfInputStatus status =
            NativeMethods.InputAddToggle(
                _nativeBridge,
                (float)physical.X,
                (float)physical.Y,
                (float)physical.Width,
                (float)physical.Height,
                out IntPtr handle);

        ThrowIfFailed(
            status,
            "toggle");

        return new WpfToggle(
            this,
            handle);
    }

    public WpfSlider AddSlider(
        Rect boundsDip)
    {
        EnsureAttached();

        Rect physical =
            _host!.DipToPhysical(
                boundsDip);

        WpfInputStatus status =
            NativeMethods.InputAddSlider(
                _nativeBridge,
                (float)physical.X,
                (float)physical.Y,
                (float)physical.Width,
                (float)physical.Height,
                out IntPtr handle);

        ThrowIfFailed(
            status,
            "slider");

        return new WpfSlider(
            this,
            handle);
    }

    public void Detach()
    {
        if (_disposed)
        {
            return;
        }

        NativeMethods.InputDetach(
            _nativeBridge);

        _host =
            null;
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        NativeMethods.InputDetach(
            _nativeBridge);

        NativeMethods.InputDestroy(
            _nativeBridge);

        _nativeBridge =
            IntPtr.Zero;

        _host =
            null;

        _disposed =
            true;

        GC.SuppressFinalize(
            this);
    }

    internal Rect ToPhysical(
        Rect boundsDip)
    {
        EnsureAttached();

        return _host!.DipToPhysical(
            boundsDip);
    }

    internal void EnsureUsable()
    {
        ObjectDisposedException.ThrowIf(
            _disposed,
            this);

        if (_nativeBridge == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "Native input bridge is unavailable.");
        }
    }

    private void EnsureAttached()
    {
        EnsureUsable();

        if (_host is null ||
            !IsAttached)
        {
            throw new InvalidOperationException(
                "Attach the WPF input bridge before using controls.");
        }
    }

    private static void ThrowIfFailed(
        WpfInputStatus status,
        string operation)
    {
        if (status == WpfInputStatus.Ok)
        {
            return;
        }

        throw new InvalidOperationException(
            "AuroraGlass WPF input operation failed: " +
            operation +
            " (" +
            status +
            ").");
    }

    public sealed class WpfButton
    {
        private readonly WpfControlInputBridge
            _owner;

        private readonly IntPtr
            _handle;

        internal WpfButton(
            WpfControlInputBridge owner,
            IntPtr handle)
        {
            _owner =
                owner;

            _handle =
                handle;
        }

        public uint ClickCount
        {
            get
            {
                _owner.EnsureUsable();

                return NativeMethods.ButtonClickCount(
                    _handle);
            }
        }

        public bool IsPressed
        {
            get
            {
                _owner.EnsureUsable();

                return NativeMethods.ButtonIsPressed(
                    _handle) != 0;
            }
        }

        public void SetBounds(
            Rect boundsDip)
        {
            Rect physical =
                _owner.ToPhysical(
                    boundsDip);

            WpfInputStatus status =
                NativeMethods.ButtonSetBounds(
                    _handle,
                    (float)physical.X,
                    (float)physical.Y,
                    (float)physical.Width,
                    (float)physical.Height);

            ThrowIfFailed(
                status,
                "button bounds");
        }
    }

    public sealed class WpfToggle
    {
        private readonly WpfControlInputBridge
            _owner;

        private readonly IntPtr
            _handle;

        internal WpfToggle(
            WpfControlInputBridge owner,
            IntPtr handle)
        {
            _owner =
                owner;

            _handle =
                handle;
        }

        public bool IsChecked
        {
            get
            {
                _owner.EnsureUsable();

                return NativeMethods.ToggleIsChecked(
                    _handle) != 0;
            }
        }

        public bool IsPressed
        {
            get
            {
                _owner.EnsureUsable();

                return NativeMethods.ToggleIsPressed(
                    _handle) != 0;
            }
        }

        public void SetBounds(
            Rect boundsDip)
        {
            Rect physical =
                _owner.ToPhysical(
                    boundsDip);

            WpfInputStatus status =
                NativeMethods.ToggleSetBounds(
                    _handle,
                    (float)physical.X,
                    (float)physical.Y,
                    (float)physical.Width,
                    (float)physical.Height);

            ThrowIfFailed(
                status,
                "toggle bounds");
        }
    }

    public sealed class WpfSlider
    {
        private readonly WpfControlInputBridge
            _owner;

        private readonly IntPtr
            _handle;

        internal WpfSlider(
            WpfControlInputBridge owner,
            IntPtr handle)
        {
            _owner =
                owner;

            _handle =
                handle;
        }

        public float Value
        {
            get
            {
                _owner.EnsureUsable();

                return NativeMethods.SliderValue(
                    _handle);
            }
        }

        public bool IsPressed
        {
            get
            {
                _owner.EnsureUsable();

                return NativeMethods.SliderIsPressed(
                    _handle) != 0;
            }
        }

        public void SetBounds(
            Rect boundsDip)
        {
            Rect physical =
                _owner.ToPhysical(
                    boundsDip);

            WpfInputStatus status =
                NativeMethods.SliderSetBounds(
                    _handle,
                    (float)physical.X,
                    (float)physical.Y,
                    (float)physical.Width,
                    (float)physical.Height);

            ThrowIfFailed(
                status,
                "slider bounds");
        }
    }

    private static class NativeMethods
    {
        private const string NativeLibrary =
            "AuroraGlassWpfInterop.dll";

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfInputCreate",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr InputCreate();

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfInputDestroy",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern void InputDestroy(
            IntPtr bridge);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfInputAttach",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WpfInputStatus InputAttach(
            IntPtr bridge,
            IntPtr hwnd);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfInputDetach",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern void InputDetach(
            IntPtr bridge);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfInputIsAttached",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int InputIsAttached(
            IntPtr bridge);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfInputOwnsCapture",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int InputOwnsCapture(
            IntPtr bridge);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfInputAddButton",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WpfInputStatus InputAddButton(
            IntPtr bridge,
            float x,
            float y,
            float width,
            float height,
            out IntPtr button);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfInputAddToggle",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WpfInputStatus InputAddToggle(
            IntPtr bridge,
            float x,
            float y,
            float width,
            float height,
            out IntPtr toggle);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfInputAddSlider",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WpfInputStatus InputAddSlider(
            IntPtr bridge,
            float x,
            float y,
            float width,
            float height,
            out IntPtr slider);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfButtonSetBounds",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WpfInputStatus ButtonSetBounds(
            IntPtr button,
            float x,
            float y,
            float width,
            float height);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfButtonClickCount",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint ButtonClickCount(
            IntPtr button);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfButtonIsPressed",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int ButtonIsPressed(
            IntPtr button);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfToggleSetBounds",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WpfInputStatus ToggleSetBounds(
            IntPtr toggle,
            float x,
            float y,
            float width,
            float height);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfToggleIsChecked",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int ToggleIsChecked(
            IntPtr toggle);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfToggleIsPressed",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int ToggleIsPressed(
            IntPtr toggle);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfSliderSetBounds",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WpfInputStatus SliderSetBounds(
            IntPtr slider,
            float x,
            float y,
            float width,
            float height);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfSliderValue",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern float SliderValue(
            IntPtr slider);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWpfSliderIsPressed",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SliderIsPressed(
            IntPtr slider);
    }
}
