using System.Runtime.InteropServices;

namespace AuroraGlass.WinUI;

public readonly record struct WinUIGlassMaterialSnapshot(
    float BlurRadius,
    float RefractionStrength,
    float DispersionStrength,
    float Thickness,
    float EdgeFresnel,
    float SpecularStrength,
    float TintAmount,
    float Saturation,
    float Brightness,
    float NoiseAmount,
    float CornerRadius,
    float Opacity,
    float HighlightX,
    float HighlightY);

public sealed class WinUIGlassMaterial : IDisposable
{
    private nint handle;
    private bool disposed;

    public WinUIGlassMaterial()
    {
        handle = Native.Create();

        if (handle == 0)
        {
            throw new InvalidOperationException(
                "AuroraGlassWinUIMaterialCreate failed.");
        }
    }

    public WinUIGlassMaterialSnapshot Snapshot
    {
        get
        {
            EnsureUsable();

            int status =
                Native.GetSnapshot(
                    handle,
                    out NativeSnapshot value);

            ThrowIfFailed(
                status,
                "GetSnapshot");

            return new WinUIGlassMaterialSnapshot(
                value.BlurRadius,
                value.RefractionStrength,
                value.DispersionStrength,
                value.Thickness,
                value.EdgeFresnel,
                value.SpecularStrength,
                value.TintAmount,
                value.Saturation,
                value.Brightness,
                value.NoiseAmount,
                value.CornerRadius,
                value.Opacity,
                value.HighlightX,
                value.HighlightY);
        }
    }

    public void SetBlurRadius(float value) => Set(Native.SetBlurRadius, value, "BlurRadius");
    public void SetRefractionStrength(float value) => Set(Native.SetRefractionStrength, value, "RefractionStrength");
    public void SetDispersionStrength(float value) => Set(Native.SetDispersionStrength, value, "DispersionStrength");
    public void SetThickness(float value) => Set(Native.SetThickness, value, "Thickness");
    public void SetEdgeFresnel(float value) => Set(Native.SetEdgeFresnel, value, "EdgeFresnel");
    public void SetSpecularStrength(float value) => Set(Native.SetSpecularStrength, value, "SpecularStrength");
    public void SetTintAmount(float value) => Set(Native.SetTintAmount, value, "TintAmount");
    public void SetSaturation(float value) => Set(Native.SetSaturation, value, "Saturation");
    public void SetBrightness(float value) => Set(Native.SetBrightness, value, "Brightness");
    public void SetNoiseAmount(float value) => Set(Native.SetNoiseAmount, value, "NoiseAmount");
    public void SetCornerRadius(float value) => Set(Native.SetCornerRadius, value, "CornerRadius");
    public void SetOpacity(float value) => Set(Native.SetOpacity, value, "Opacity");

    public void SetHighlightPosition(
        float x,
        float y)
    {
        EnsureUsable();

        ThrowIfFailed(
            Native.SetHighlightPosition(
                handle,
                x,
                y),
            "HighlightPosition");
    }

    public void Dispose()
    {
        if (disposed)
        {
            return;
        }

        Native.Destroy(
            handle);

        handle = 0;
        disposed = true;

        GC.SuppressFinalize(
            this);
    }

    private delegate int Setter(
        nint material,
        float value);

    private void Set(
        Setter setter,
        float value,
        string name)
    {
        EnsureUsable();

        ThrowIfFailed(
            setter(
                handle,
                value),
            name);
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
        if (status == 0)
        {
            return;
        }

        throw new ArgumentOutOfRangeException(
            operation,
            "AuroraGlass material validation rejected the value. Status=" +
            status);
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeSnapshot
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

    private static class Native
    {
        private const string Dll =
            "AuroraGlassWinUIInterop.dll";

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialCreate", CallingConvention = CallingConvention.Cdecl)]
        internal static extern nint Create();

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialDestroy", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void Destroy(nint material);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialGetSnapshot", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int GetSnapshot(nint material, out NativeSnapshot snapshot);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialSetBlurRadius", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetBlurRadius(nint material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialSetRefractionStrength", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetRefractionStrength(nint material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialSetDispersionStrength", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetDispersionStrength(nint material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialSetThickness", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetThickness(nint material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialSetEdgeFresnel", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetEdgeFresnel(nint material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialSetSpecularStrength", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetSpecularStrength(nint material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialSetTintAmount", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetTintAmount(nint material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialSetSaturation", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetSaturation(nint material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialSetBrightness", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetBrightness(nint material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialSetNoiseAmount", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetNoiseAmount(nint material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialSetCornerRadius", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetCornerRadius(nint material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialSetOpacity", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetOpacity(nint material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWinUIMaterialSetHighlightPosition", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int SetHighlightPosition(nint material, float x, float y);
    }
}