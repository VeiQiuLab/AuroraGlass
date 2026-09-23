using System.Runtime.InteropServices;
using Windows.Foundation;

namespace AuroraGlass.WinUI;

public readonly record struct WinUIRenderHostStats(
    ulong FrameCount,
    uint Width,
    uint Height,
    int LastCoreStatus,
    bool Ready);

public sealed class WinUIRenderHost : IDisposable
{
    private readonly WinUIHostAttachment host;

    private nint handle;
    private nint child;
    private bool disposed;

    public WinUIRenderHost(
        WinUIHostAttachment host)
    {
        ArgumentNullException.ThrowIfNull(
            host);

        if (!host.IsAttached ||
            host.WindowHandle == 0)
        {
            throw new InvalidOperationException(
                "WinUIHostAttachment must already be attached.");
        }

        this.host = host;

        handle =
            Native.Create(
                host.WindowHandle);

        if (handle == 0)
        {
            throw new InvalidOperationException(
                "AuroraGlass WinUI render host creation failed.");
        }

        child =
            Native.GetHwnd(
                handle);

        if (child == 0)
        {
            Native.Destroy(
                handle);

            handle = 0;

            throw new InvalidOperationException(
                "AuroraGlass render child HWND is unavailable.");
        }

        host.MetricsChanged += OnMetricsChanged;

        ResizeFromMetrics(
            host.CurrentMetrics,
            true);
    }

    public nint ChildWindowHandle =>
        child;

    public int LastResizeStatus
    {
        get;
        private set;
    }

    public WinUIRenderHostStats Stats
    {
        get
        {
            EnsureUsable();

            ThrowIfFailed(
                Native.GetStats(
                    handle,
                    out NativeStats value),
                "GetStats");

            return new WinUIRenderHostStats(
                value.FrameCount,
                value.Width,
                value.Height,
                value.LastCoreStatus,
                value.Ready != 0);
        }
    }

    public void ResizeNow() =>
        ResizeFromMetrics(
            host.CurrentMetrics,
            true);

    public void SetMaterial(
        WinUIGlassMaterial material)
    {
        ArgumentNullException.ThrowIfNull(
            material);

        EnsureUsable();

        WinUIGlassMaterialSnapshot value =
            material.Snapshot;

        NativeMaterial mapped =
            new NativeMaterial
            {
                BlurRadius = value.BlurRadius,
                RefractionStrength = value.RefractionStrength,
                DispersionStrength = value.DispersionStrength,
                Thickness = value.Thickness,
                EdgeFresnel = value.EdgeFresnel,
                SpecularStrength = value.SpecularStrength,
                TintAmount = value.TintAmount,
                Saturation = value.Saturation,
                Brightness = value.Brightness,
                NoiseAmount = value.NoiseAmount,
                CornerRadius = value.CornerRadius,
                Opacity = value.Opacity,
                HighlightX = value.HighlightX,
                HighlightY = value.HighlightY
            };

        ThrowIfFailed(
            Native.SetMaterial(
                handle,
                ref mapped),
            "SetMaterial");
    }

    public void SetLogicalRects(
        params Rect[] rects)
    {
        ArgumentNullException.ThrowIfNull(
            rects);

        EnsureUsable();

        NativeRect[] mapped =
            new NativeRect[
                rects.Length];

        for (int i = 0;
             i < rects.Length;
             ++i)
        {
            Rect physical =
                host.LogicalToPhysical(
                    rects[i]);

            mapped[i] =
                new NativeRect
                {
                    X = (float)physical.X,
                    Y = (float)physical.Y,
                    Width = (float)physical.Width,
                    Height = (float)physical.Height
                };
        }

        ThrowIfFailed(
            Native.SetRects(
                handle,
                mapped.Length == 0
                    ? null
                    : mapped,
                (uint)mapped.Length),
            "SetRects");
    }

    public void Dispose()
    {
        if (disposed)
        {
            return;
        }

        host.MetricsChanged -= OnMetricsChanged;

        if (handle != 0)
        {
            Native.Destroy(
                handle);

            handle = 0;
        }

        child = 0;
        disposed = true;

        GC.SuppressFinalize(
            this);
    }

    private void OnMetricsChanged(
        WinUIHostMetrics metrics)
    {
        if (!disposed &&
            handle != 0)
        {
            ResizeFromMetrics(
                metrics,
                false);
        }
    }

    private void ResizeFromMetrics(
        WinUIHostMetrics metrics,
        bool throwOnFailure)
    {
        LastResizeStatus =
            Native.Resize(
                handle,
                metrics.ClientWidth,
                metrics.ClientHeight);

        if (throwOnFailure)
        {
            ThrowIfFailed(
                LastResizeStatus,
                "Resize");
        }
    }

    private void EnsureUsable()
    {
        ObjectDisposedException.ThrowIf(
            disposed,
            this);
    }

    private static void ThrowIfFailed(
        int status,
        string operation)
    {
        if (status != 0)
        {
            throw new InvalidOperationException(
                operation +
                " failed. Status=" +
                status);
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeMaterial
    {
        internal float BlurRadius;
        internal float RefractionStrength;
        internal float DispersionStrength;
        internal float Thickness;
        internal float EdgeFresnel;
        internal float SpecularStrength;
        internal float TintAmount;
        internal float Saturation;
        internal float Brightness;
        internal float NoiseAmount;
        internal float CornerRadius;
        internal float Opacity;
        internal float HighlightX;
        internal float HighlightY;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeRect
    {
        internal float X;
        internal float Y;
        internal float Width;
        internal float Height;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeStats
    {
        internal ulong FrameCount;
        internal uint Width;
        internal uint Height;
        internal int LastCoreStatus;
        internal uint Ready;
    }

    private static class Native
    {
        private const string Dll =
            "AuroraGlassWinUIInterop.dll";

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIRenderHostCreate", CallingConvention = CallingConvention.Cdecl)]
        internal static extern nint Create(nint parent);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIRenderHostDestroy", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void Destroy(nint host);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIRenderHostGetHwnd", CallingConvention = CallingConvention.Cdecl)]
        internal static extern nint GetHwnd(nint host);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIRenderHostResize", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int Resize(nint host, uint width, uint height);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIRenderHostSetMaterial", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetMaterial(nint host, ref NativeMaterial material);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIRenderHostSetRects", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetRects(nint host, [In] NativeRect[]? rects, uint count);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIRenderHostGetStats", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int GetStats(nint host, out NativeStats stats);
    }
}