using System.Runtime.InteropServices;

namespace AuroraGlass.WinUI;

internal static class NativeMethods
{
    private const string DllName =
        "AuroraGlassWinUIInterop.dll";

    [StructLayout(LayoutKind.Sequential)]
    internal struct NativeHostMetrics
    {
        internal uint ClientWidth;
        internal uint ClientHeight;
        internal uint Dpi;
    }

    [UnmanagedFunctionPointer(
        CallingConvention.Cdecl)]
    internal delegate void HostMetricsCallback(
        nint metrics,
        nint userData);

    [DllImport(
        DllName,
        CallingConvention = CallingConvention.Cdecl)]
    internal static extern nint
        AuroraGlassWinUIHostCreate();

    [DllImport(
        DllName,
        CallingConvention = CallingConvention.Cdecl)]
    internal static extern void
        AuroraGlassWinUIHostDestroy(
            nint host);

    [DllImport(
        DllName,
        CallingConvention = CallingConvention.Cdecl)]
    internal static extern int
        AuroraGlassWinUIHostAttach(
            nint host,
            nint hwnd);

    [DllImport(
        DllName,
        CallingConvention = CallingConvention.Cdecl)]
    internal static extern void
        AuroraGlassWinUIHostDetach(
            nint host);

    [DllImport(
        DllName,
        CallingConvention = CallingConvention.Cdecl)]
    internal static extern int
        AuroraGlassWinUIHostIsAttached(
            nint host);

    [DllImport(
        DllName,
        CallingConvention = CallingConvention.Cdecl)]
    internal static extern int
        AuroraGlassWinUIHostGetMetrics(
            nint host,
            out NativeHostMetrics metrics);

    [DllImport(
        DllName,
        CallingConvention = CallingConvention.Cdecl)]
    internal static extern int
        AuroraGlassWinUIHostSetMetricsCallback(
            nint host,
            HostMetricsCallback? callback,
            nint userData);
}