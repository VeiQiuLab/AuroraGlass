using System.Runtime.InteropServices;
using Windows.Foundation;

namespace AuroraGlass.WinUI;

public enum WinUIInputStatus
{
    Ok = 0,
    InvalidArgument = 1,
    InvalidWindow = 2,
    DifferentWindowAlreadyAttached = 3,
    AttachFailed = 4,
    AddControlFailed = 5
}

public sealed class WinUIControlInputBridge : IDisposable
{
    private IntPtr _nativeBridge;
    private WinUIHostAttachment? _host;
    private bool _disposed;

    public WinUIControlInputBridge()
    {
        _nativeBridge =
            NativeMethods.InputCreate();

        if (_nativeBridge == IntPtr.Zero)
        {
            throw new OutOfMemoryException(
                "Failed to create AuroraGlass WinUI 3 input bridge.");
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

    public WinUIInputStatus Attach(
        WinUIHostAttachment host)
    {
        ObjectDisposedException.ThrowIf(
            _disposed,
            this);

        ArgumentNullException.ThrowIfNull(
            host);

        if (!host.IsAttached ||
            host.WindowHandle == IntPtr.Zero)
        {
            return WinUIInputStatus.InvalidWindow;
        }

        WinUIInputStatus status =
            NativeMethods.InputAttach(
                _nativeBridge,
                host.WindowHandle);

        if (status == WinUIInputStatus.Ok)
        {
            _host =
                host;
        }

        return status;
    }

    public WinUIButton AddButton(
        Rect boundsDip)
    {
        EnsureAttached();

        Rect physical =
            _host!.LogicalToPhysical(
                boundsDip);

        WinUIInputStatus status =
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

        return new WinUIButton(
            this,
            handle);
    }

    public WinUIToggle AddToggle(
        Rect boundsDip)
    {
        EnsureAttached();

        Rect physical =
            _host!.LogicalToPhysical(
                boundsDip);

        WinUIInputStatus status =
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

        return new WinUIToggle(
            this,
            handle);
    }

    public WinUISlider AddSlider(
        Rect boundsDip)
    {
        EnsureAttached();

        Rect physical =
            _host!.LogicalToPhysical(
                boundsDip);

        WinUIInputStatus status =
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

        return new WinUISlider(
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

        return _host!.LogicalToPhysical(
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
                "Attach the WinUI 3 input bridge before using controls.");
        }
    }

    private static void ThrowIfFailed(
        WinUIInputStatus status,
        string operation)
    {
        if (status == WinUIInputStatus.Ok)
        {
            return;
        }

        throw new InvalidOperationException(
            "AuroraGlass WinUI 3 input operation failed: " +
            operation +
            " (" +
            status +
            ").");
    }

    public sealed class WinUIButton
    {
        private readonly WinUIControlInputBridge
            _owner;

        private readonly IntPtr
            _handle;

        internal WinUIButton(
            WinUIControlInputBridge owner,
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

            WinUIInputStatus status =
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

    public sealed class WinUIToggle
    {
        private readonly WinUIControlInputBridge
            _owner;

        private readonly IntPtr
            _handle;

        internal WinUIToggle(
            WinUIControlInputBridge owner,
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

            WinUIInputStatus status =
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

    public sealed class WinUISlider
    {
        private readonly WinUIControlInputBridge
            _owner;

        private readonly IntPtr
            _handle;

        internal WinUISlider(
            WinUIControlInputBridge owner,
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

            WinUIInputStatus status =
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
            "AuroraGlassWinUIInterop.dll";

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIInputCreate",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr InputCreate();

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIInputDestroy",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern void InputDestroy(
            IntPtr bridge);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIInputAttach",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WinUIInputStatus InputAttach(
            IntPtr bridge,
            IntPtr hwnd);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIInputDetach",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern void InputDetach(
            IntPtr bridge);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIInputIsAttached",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int InputIsAttached(
            IntPtr bridge);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIInputOwnsCapture",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int InputOwnsCapture(
            IntPtr bridge);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIInputAddButton",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WinUIInputStatus InputAddButton(
            IntPtr bridge,
            float x,
            float y,
            float width,
            float height,
            out IntPtr button);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIInputAddToggle",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WinUIInputStatus InputAddToggle(
            IntPtr bridge,
            float x,
            float y,
            float width,
            float height,
            out IntPtr toggle);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIInputAddSlider",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WinUIInputStatus InputAddSlider(
            IntPtr bridge,
            float x,
            float y,
            float width,
            float height,
            out IntPtr slider);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIButtonSetBounds",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WinUIInputStatus ButtonSetBounds(
            IntPtr button,
            float x,
            float y,
            float width,
            float height);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIButtonClickCount",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint ButtonClickCount(
            IntPtr button);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIButtonIsPressed",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int ButtonIsPressed(
            IntPtr button);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIToggleSetBounds",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WinUIInputStatus ToggleSetBounds(
            IntPtr toggle,
            float x,
            float y,
            float width,
            float height);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIToggleIsChecked",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int ToggleIsChecked(
            IntPtr toggle);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUIToggleIsPressed",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int ToggleIsPressed(
            IntPtr toggle);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUISliderSetBounds",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern WinUIInputStatus SliderSetBounds(
            IntPtr slider,
            float x,
            float y,
            float width,
            float height);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUISliderValue",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern float SliderValue(
            IntPtr slider);

        [DllImport(
            NativeLibrary,
            EntryPoint = "AuroraGlassWinUISliderIsPressed",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SliderIsPressed(
            IntPtr slider);
    }
}
