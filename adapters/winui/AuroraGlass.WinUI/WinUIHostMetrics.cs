namespace AuroraGlass.WinUI;

public readonly record struct WinUIHostMetrics(
    uint ClientWidth,
    uint ClientHeight,
    uint Dpi)
{
    public bool IsZeroSize =>
        ClientWidth == 0 &&
        ClientHeight == 0;

    public double LogicalToPhysicalScale =>
        Dpi == 0
            ? 1.0
            : Dpi / 96.0;
}