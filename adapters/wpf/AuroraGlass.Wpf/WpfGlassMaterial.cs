using System.Runtime.InteropServices;

namespace AuroraGlass.Wpf;

public readonly record struct WpfGlassMaterialSnapshot(
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

[StructLayout(LayoutKind.Sequential)]
internal struct NativeMaterialSnapshot
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

    internal readonly WpfGlassMaterialSnapshot ToPublic()
    {
        return new WpfGlassMaterialSnapshot(
            BlurRadius,
            RefractionStrength,
            DispersionStrength,
            Thickness,
            EdgeFresnel,
            SpecularStrength,
            TintAmount,
            Saturation,
            Brightness,
            NoiseAmount,
            CornerRadius,
            Opacity,
            HighlightX,
            HighlightY);
    }
}

public sealed class WpfGlassMaterial : IDisposable
{
    private IntPtr _native;
    private bool _disposed;

    public WpfGlassMaterial()
    {
        _native =
            NativeMethods.MaterialCreate();

        if (_native == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "AuroraGlassWpfMaterialCreate failed.");
        }
    }

    public WpfGlassMaterialSnapshot Snapshot
    {
        get
        {
            EnsureAlive();

            NativeMaterialSnapshot snapshot;

            ThrowIfFailed(
                NativeMethods.MaterialGetSnapshot(
                    _native,
                    out snapshot));

            return snapshot.ToPublic();
        }
    }

    internal NativeMaterialSnapshot NativeSnapshot
    {
        get
        {
            EnsureAlive();

            NativeMaterialSnapshot snapshot;

            ThrowIfFailed(
                NativeMethods.MaterialGetSnapshot(
                    _native,
                    out snapshot));

            return snapshot;
        }
    }

    public void SetBlurRadius(float value) =>
        Set(
            NativeMethods.MaterialSetBlurRadius(
                _native,
                value));

    public void SetRefractionStrength(float value) =>
        Set(
            NativeMethods.MaterialSetRefractionStrength(
                _native,
                value));

    public void SetDispersionStrength(float value) =>
        Set(
            NativeMethods.MaterialSetDispersionStrength(
                _native,
                value));

    public void SetThickness(float value) =>
        Set(
            NativeMethods.MaterialSetThickness(
                _native,
                value));

    public void SetEdgeFresnel(float value) =>
        Set(
            NativeMethods.MaterialSetEdgeFresnel(
                _native,
                value));

    public void SetSpecularStrength(float value) =>
        Set(
            NativeMethods.MaterialSetSpecularStrength(
                _native,
                value));

    public void SetTintAmount(float value) =>
        Set(
            NativeMethods.MaterialSetTintAmount(
                _native,
                value));

    public void SetSaturation(float value) =>
        Set(
            NativeMethods.MaterialSetSaturation(
                _native,
                value));

    public void SetBrightness(float value) =>
        Set(
            NativeMethods.MaterialSetBrightness(
                _native,
                value));

    public void SetNoiseAmount(float value) =>
        Set(
            NativeMethods.MaterialSetNoiseAmount(
                _native,
                value));

    public void SetCornerRadius(float value) =>
        Set(
            NativeMethods.MaterialSetCornerRadius(
                _native,
                value));

    public void SetOpacity(float value) =>
        Set(
            NativeMethods.MaterialSetOpacity(
                _native,
                value));

    public void SetHighlightPosition(
        float x,
        float y) =>
        Set(
            NativeMethods.MaterialSetHighlightPosition(
                _native,
                x,
                y));

    private void Set(int status)
    {
        EnsureAlive();
        ThrowIfFailed(status);
    }

    private static void ThrowIfFailed(int status)
    {
        if (status != 0)
        {
            throw new InvalidOperationException(
                "AuroraGlass GlassMaterial rejected the value. Status=" +
                status);
        }
    }

    private void EnsureAlive()
    {
        ObjectDisposedException.ThrowIf(
            _disposed,
            this);
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        _disposed = true;

        NativeMethods.MaterialDestroy(
            _native);

        _native = IntPtr.Zero;

        GC.SuppressFinalize(this);
    }

    private static class NativeMethods
    {
        private const string Dll =
            "AuroraGlassWpfInterop";

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialCreate")]
        internal static extern IntPtr MaterialCreate();

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialDestroy")]
        internal static extern void MaterialDestroy(
            IntPtr material);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialGetSnapshot")]
        internal static extern int MaterialGetSnapshot(
            IntPtr material,
            out NativeMaterialSnapshot snapshot);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialSetBlurRadius")]
        internal static extern int MaterialSetBlurRadius(IntPtr material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialSetRefractionStrength")]
        internal static extern int MaterialSetRefractionStrength(IntPtr material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialSetDispersionStrength")]
        internal static extern int MaterialSetDispersionStrength(IntPtr material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialSetThickness")]
        internal static extern int MaterialSetThickness(IntPtr material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialSetEdgeFresnel")]
        internal static extern int MaterialSetEdgeFresnel(IntPtr material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialSetSpecularStrength")]
        internal static extern int MaterialSetSpecularStrength(IntPtr material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialSetTintAmount")]
        internal static extern int MaterialSetTintAmount(IntPtr material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialSetSaturation")]
        internal static extern int MaterialSetSaturation(IntPtr material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialSetBrightness")]
        internal static extern int MaterialSetBrightness(IntPtr material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialSetNoiseAmount")]
        internal static extern int MaterialSetNoiseAmount(IntPtr material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialSetCornerRadius")]
        internal static extern int MaterialSetCornerRadius(IntPtr material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialSetOpacity")]
        internal static extern int MaterialSetOpacity(IntPtr material, float value);

        [DllImport(Dll, EntryPoint = "AuroraGlassWpfMaterialSetHighlightPosition")]
        internal static extern int MaterialSetHighlightPosition(IntPtr material, float x, float y);
    }
}
