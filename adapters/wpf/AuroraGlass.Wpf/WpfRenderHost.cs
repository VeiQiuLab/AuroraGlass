using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace AuroraGlass.Wpf;

public readonly record struct WpfRenderHostStats(
    ulong FrameCount,
    uint Width,
    uint Height,
    int LastCoreStatus,
    bool Ready);

[StructLayout(LayoutKind.Sequential)]
internal struct NativeRenderRect
{
    internal float X;
    internal float Y;
    internal float Width;
    internal float Height;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeRenderStats
{
    internal ulong FrameCount;
    internal uint Width;
    internal uint Height;
    internal int LastCoreStatus;
    internal uint Ready;
}

public sealed class WpfRenderHost : HwndHost
{
    private IntPtr _nativeHost;
    private IntPtr _childHwnd;

    public bool IsReady =>
        _nativeHost != IntPtr.Zero &&
        Stats.Ready;

    public WpfRenderHostStats Stats
    {
        get
        {
            if (_nativeHost == IntPtr.Zero)
            {
                return default;
            }

            NativeRenderStats stats;

            int status =
                NativeMethods.RenderHostGetStats(
                    _nativeHost,
                    out stats);

            if (status != 0)
            {
                throw new InvalidOperationException(
                    "AuroraGlassWpfRenderHostGetStats failed. Status=" +
                    status);
            }

            return new WpfRenderHostStats(
                stats.FrameCount,
                stats.Width,
                stats.Height,
                stats.LastCoreStatus,
                stats.Ready != 0);
        }
    }

    public void SetMaterial(
        WpfGlassMaterial material)
    {
        ArgumentNullException.ThrowIfNull(
            material);

        EnsureCreated();

        NativeMaterialSnapshot snapshot =
            material.NativeSnapshot;

        int status =
            NativeMethods.RenderHostSetMaterial(
                _nativeHost,
                ref snapshot);

        ThrowIfFailed(
            status,
            "SetMaterial");
    }

    public void SetPhysicalRects(
        params Rect[] rects)
    {
        ArgumentNullException.ThrowIfNull(
            rects);

        EnsureCreated();

        NativeRenderRect[] native =
            new NativeRenderRect[
                rects.Length];

        for (int i = 0;
             i < rects.Length;
             ++i)
        {
            Rect rect =
                rects[i];

            native[i] =
                new NativeRenderRect
                {
                    X = (float)rect.X,
                    Y = (float)rect.Y,
                    Width = (float)rect.Width,
                    Height = (float)rect.Height
                };
        }

        int status =
            NativeMethods.RenderHostSetRects(
                _nativeHost,
                native,
                (uint)native.Length);

        ThrowIfFailed(
            status,
            "SetRects");
    }

    protected override HandleRef BuildWindowCore(
        HandleRef hwndParent)
    {
        if (_nativeHost != IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "Render host already created.");
        }

        _nativeHost =
            NativeMethods.RenderHostCreate(
                hwndParent.Handle);

        if (_nativeHost == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "AuroraGlass native WPF render host creation failed.");
        }

        _childHwnd =
            NativeMethods.RenderHostGetHwnd(
                _nativeHost);

        if (_childHwnd == IntPtr.Zero)
        {
            NativeMethods.RenderHostDestroy(
                _nativeHost);

            _nativeHost = IntPtr.Zero;

            throw new InvalidOperationException(
                "AuroraGlass native child HWND was not created.");
        }

        return new HandleRef(
            this,
            _childHwnd);
    }

    protected override void DestroyWindowCore(
        HandleRef hwnd)
    {
        if (_nativeHost != IntPtr.Zero)
        {
            NativeMethods.RenderHostDestroy(
                _nativeHost);

            _nativeHost =
                IntPtr.Zero;
        }

        _childHwnd =
            IntPtr.Zero;
    }

    private void EnsureCreated()
    {
        if (_nativeHost == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "WpfRenderHost has not created its native child HWND yet.");
        }
    }

    private static void ThrowIfFailed(
        int status,
        string operation)
    {
        if (status != 0)
        {
            throw new InvalidOperationException(
                "AuroraGlass render host " +
                operation +
                " failed. Status=" +
                status);
        }
    }

    private static class NativeMethods
    {
        private const string Dll =
            "AuroraGlassWpfInterop";

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfRenderHostCreate")]
        internal static extern IntPtr RenderHostCreate(
            IntPtr parentHwnd);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfRenderHostDestroy")]
        internal static extern void RenderHostDestroy(
            IntPtr host);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfRenderHostGetHwnd")]
        internal static extern IntPtr RenderHostGetHwnd(
            IntPtr host);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfRenderHostSetMaterial")]
        internal static extern int RenderHostSetMaterial(
            IntPtr host,
            ref NativeMaterialSnapshot snapshot);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfRenderHostSetRects")]
        internal static extern int RenderHostSetRects(
            IntPtr host,
            [In] NativeRenderRect[] rects,
            uint count);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfRenderHostGetStats")]
        internal static extern int RenderHostGetStats(
            IntPtr host,
            out NativeRenderStats stats);
    }
}
