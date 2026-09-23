using System.Runtime.InteropServices;

namespace AuroraGlass.WinUI;

internal static class NativeMethods
{
    private const string DllName =
        "AuroraGlassWinUIInterop.dll";

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
}