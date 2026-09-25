using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Threading;

namespace AuroraGlass.Wpf;

public readonly record struct WpfGlassImageStats(
    ulong FrameCount,
    uint Width,
    uint Height,
    int LastCoreStatus,
    bool Ready);

[StructLayout(LayoutKind.Sequential)]
internal struct NativeCompositionStats
{
    internal ulong FrameCount;
    internal uint Width;
    internal uint Height;
    internal int LastCoreStatus;
    internal uint Ready;
}

/// <summary>
/// Airspace-safe WPF composition source for AuroraGlass.
///
/// Renders AuroraGlass (D3D11/HLSL) offscreen into a shared BGRA surface and
/// exposes it as a WPF <see cref="D3DImage"/> (an ordinary WPF ImageSource).
/// It does NOT create an HwndHost or a WS_CHILD native window, so ordinary WPF
/// controls can be layered above it with normal hit-testing.
///
/// This is additive: <see cref="WpfRenderHost"/> (the HwndHost path) is kept
/// unchanged for scenarios that do not overlay WPF controls.
/// </summary>
public sealed class WpfGlassImageSource : IDisposable
{
    private readonly D3DImage _image = new();
    private IntPtr _native;
    private bool _disposed;
    private bool _rendering;
    private int _pixelWidth;
    private int _pixelHeight;
    private DateTime _start = DateTime.UtcNow;

    public WpfGlassImageSource(int pixelWidth, int pixelHeight)
    {
        if (pixelWidth <= 0 || pixelHeight <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(pixelWidth));
        }

        _native = NativeMethods.CompositionCreate(
            (uint)pixelWidth,
            (uint)pixelHeight);

        if (_native == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "AuroraGlassWpfCompositionCreate failed.");
        }

        _pixelWidth = pixelWidth;
        _pixelHeight = pixelHeight;

        _image.IsFrontBufferAvailableChanged += OnFrontBufferAvailableChanged;

        BindBackBuffer();
    }

    /// <summary>The WPF ImageSource to place in the visual tree.</summary>
    public ImageSource ImageSource => _image;

    public int PixelWidth => _pixelWidth;
    public int PixelHeight => _pixelHeight;

    public WpfGlassImageStats Stats
    {
        get
        {
            if (_native == IntPtr.Zero) return default;

            int status = NativeMethods.CompositionGetStats(_native, out NativeCompositionStats s);
            if (status != 0)
            {
                throw new InvalidOperationException(
                    "AuroraGlassWpfCompositionGetStats failed. Status=" + status);
            }

            return new WpfGlassImageStats(s.FrameCount, s.Width, s.Height, s.LastCoreStatus, s.Ready != 0);
        }
    }

    public void SetMaterial(WpfGlassMaterial material)
    {
        ArgumentNullException.ThrowIfNull(material);
        EnsureAlive();

        NativeMaterialSnapshot snapshot = material.NativeSnapshot;
        int status = NativeMethods.CompositionSetMaterial(_native, ref snapshot);
        ThrowIfFailed(status, "SetMaterial");
    }

    public void SetPhysicalRects(params Rect[] rects)
    {
        ArgumentNullException.ThrowIfNull(rects);
        EnsureAlive();

        NativeRenderRect[] native = new NativeRenderRect[rects.Length];
        for (int i = 0; i < rects.Length; ++i)
        {
            Rect r = rects[i];
            native[i] = new NativeRenderRect
            {
                X = (float)r.X,
                Y = (float)r.Y,
                Width = (float)r.Width,
                Height = (float)r.Height,
            };
        }

        int status = NativeMethods.CompositionSetRects(_native, native, (uint)native.Length);
        ThrowIfFailed(status, "SetRects");
    }

    /// <summary>Resize the offscreen shared surface (physical pixels).</summary>
    public void Resize(int pixelWidth, int pixelHeight)
    {
        if (pixelWidth <= 0 || pixelHeight <= 0) return;
        EnsureAlive();

        if (pixelWidth == _pixelWidth && pixelHeight == _pixelHeight) return;

        int status = NativeMethods.CompositionResize(_native, (uint)pixelWidth, (uint)pixelHeight);
        ThrowIfFailed(status, "Resize");

        _pixelWidth = pixelWidth;
        _pixelHeight = pixelHeight;
        BindBackBuffer();
        RenderFrame();
    }

    /// <summary>Start the CompositionTarget.Rendering-driven frame loop.</summary>
    public void Start()
    {
        if (_rendering) return;
        _rendering = true;
        _start = DateTime.UtcNow;
        CompositionTarget.Rendering += OnRendering;
    }

    public void Stop()
    {
        if (!_rendering) return;
        _rendering = false;
        CompositionTarget.Rendering -= OnRendering;
    }

    /// <summary>Render a single frame now (also used by the frame loop).</summary>
    public void RenderFrame()
    {
        if (_disposed || _native == IntPtr.Zero) return;

        // Do not render while WPF has no front buffer to present to.
        if (!_image.IsFrontBufferAvailable) return;
        if (_pixelWidth == 0 || _pixelHeight == 0) return;

        float time = (float)(DateTime.UtcNow - _start).TotalSeconds;
        int status = NativeMethods.CompositionRender(_native, time);
        if (status != 0) return;

        if (_image.IsFrontBufferAvailable)
        {
            _image.Lock();
            try
            {
                _image.AddDirtyRect(new Int32Rect(0, 0, _pixelWidth, _pixelHeight));
            }
            finally
            {
                _image.Unlock();
            }
        }
    }

    private void OnRendering(object? sender, EventArgs e) => RenderFrame();

    private void OnFrontBufferAvailableChanged(object? sender, DependencyPropertyChangedEventArgs e)
    {
        if (_image.IsFrontBufferAvailable)
        {
            // Front buffer came back (e.g. after lock/session change): rebind + redraw.
            BindBackBuffer();
            RenderFrame();
        }
    }

    private void BindBackBuffer()
    {
        IntPtr surface = NativeMethods.CompositionGetSurface(_native);
        if (surface == IntPtr.Zero) return;

        _image.Lock();
        try
        {
            _image.SetBackBuffer(D3DResourceType.IDirect3DSurface9, surface, enableSoftwareFallback: false);
        }
        finally
        {
            _image.Unlock();
        }
    }

    private void EnsureAlive()
    {
        if (_disposed) throw new ObjectDisposedException(nameof(WpfGlassImageSource));
        if (_native == IntPtr.Zero)
        {
            throw new InvalidOperationException("WpfGlassImageSource has no native composition.");
        }
    }

    private static void ThrowIfFailed(int status, string op)
    {
        if (status != 0)
        {
            throw new InvalidOperationException(
                "AuroraGlass WPF composition " + op + " failed. Status=" + status);
        }
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;

        Stop();
        _image.IsFrontBufferAvailableChanged -= OnFrontBufferAvailableChanged;

        try
        {
            _image.Lock();
            try { _image.SetBackBuffer(D3DResourceType.IDirect3DSurface9, IntPtr.Zero); }
            finally { _image.Unlock(); }
        }
        catch { /* ignore teardown races */ }

        if (_native != IntPtr.Zero)
        {
            NativeMethods.CompositionDestroy(_native);
            _native = IntPtr.Zero;
        }
    }

    private static class NativeMethods
    {
        private const string Dll = "AuroraGlassWpfInterop";

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfCompositionCreate")]
        internal static extern IntPtr CompositionCreate(uint width, uint height);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfCompositionDestroy")]
        internal static extern void CompositionDestroy(IntPtr composition);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfCompositionGetSurface")]
        internal static extern IntPtr CompositionGetSurface(IntPtr composition);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfCompositionResize")]
        internal static extern int CompositionResize(IntPtr composition, uint width, uint height);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfCompositionSetMaterial")]
        internal static extern int CompositionSetMaterial(IntPtr composition, ref NativeMaterialSnapshot snapshot);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfCompositionSetRects")]
        internal static extern int CompositionSetRects(IntPtr composition, [In] NativeRenderRect[] rects, uint count);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfCompositionRender")]
        internal static extern int CompositionRender(IntPtr composition, float timeSeconds);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfCompositionGetStats")]
        internal static extern int CompositionGetStats(IntPtr composition, out NativeCompositionStats stats);
    }
}
