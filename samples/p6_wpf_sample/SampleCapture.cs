using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media.Imaging;
using System.IO;

namespace P6.WpfSample;

internal static class SampleCapture
{
    internal static void Capture(
        Window window,
        string path)
    {
        IntPtr hwnd =
            new WindowInteropHelper(
                window).Handle;

        if (!SampleNativeMethods.GetWindowRect(
                hwnd,
                out NativeRect rect))
        {
            throw new InvalidOperationException(
                "GetWindowRect failed.");
        }

        int width =
            rect.Right - rect.Left;

        int height =
            rect.Bottom - rect.Top;

        IntPtr screen =
            SampleNativeMethods.GetDC(
                IntPtr.Zero);

        IntPtr memory =
            SampleNativeMethods.CreateCompatibleDC(
                screen);

        IntPtr bitmap =
            SampleNativeMethods.CreateCompatibleBitmap(
                screen,
                width,
                height);

        IntPtr old =
            SampleNativeMethods.SelectObject(
                memory,
                bitmap);

        try
        {
            const uint Srccopy =
                0x00CC0020;

            const uint CaptureBlt =
                0x40000000;

            if (!SampleNativeMethods.BitBlt(
                    memory,
                    0,
                    0,
                    width,
                    height,
                    screen,
                    rect.Left,
                    rect.Top,
                    Srccopy | CaptureBlt))
            {
                throw new InvalidOperationException(
                    "BitBlt failed.");
            }

            BitmapSource source =
                Imaging.CreateBitmapSourceFromHBitmap(
                    bitmap,
                    IntPtr.Zero,
                    Int32Rect.Empty,
                    BitmapSizeOptions.FromEmptyOptions());

            source.Freeze();

            string full =
                Path.GetFullPath(path);

            string? directory =
                Path.GetDirectoryName(full);

            if (!string.IsNullOrEmpty(
                    directory))
            {
                Directory.CreateDirectory(
                    directory);
            }

            PngBitmapEncoder encoder =
                new();

            encoder.Frames.Add(
                BitmapFrame.Create(
                    source));

            using FileStream stream =
                File.Create(full);

            encoder.Save(stream);
        }
        finally
        {
            SampleNativeMethods.SelectObject(
                memory,
                old);

            SampleNativeMethods.DeleteObject(
                bitmap);

            SampleNativeMethods.DeleteDC(
                memory);

            SampleNativeMethods.ReleaseDC(
                IntPtr.Zero,
                screen);
        }
    }
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeRect
{
    internal int Left;
    internal int Top;
    internal int Right;
    internal int Bottom;
}

internal static class SampleNativeMethods
{
    [DllImport(
        "user32.dll",
        EntryPoint = "SendMessageW")]
    private static extern nint SendMessageNative(
        IntPtr hwnd,
        uint message,
        nuint wParam,
        nint lParam);

    [DllImport("user32.dll")]
    internal static extern bool GetWindowRect(
        IntPtr hwnd,
        out NativeRect rect);

    [DllImport("user32.dll")]
    internal static extern IntPtr GetDC(
        IntPtr hwnd);

    [DllImport("user32.dll")]
    internal static extern int ReleaseDC(
        IntPtr hwnd,
        IntPtr dc);

    [DllImport("gdi32.dll")]
    internal static extern IntPtr CreateCompatibleDC(
        IntPtr dc);

    [DllImport("gdi32.dll")]
    internal static extern IntPtr CreateCompatibleBitmap(
        IntPtr dc,
        int width,
        int height);

    [DllImport("gdi32.dll")]
    internal static extern IntPtr SelectObject(
        IntPtr dc,
        IntPtr obj);

    [DllImport("gdi32.dll")]
    internal static extern bool DeleteObject(
        IntPtr obj);

    [DllImport("gdi32.dll")]
    internal static extern bool DeleteDC(
        IntPtr dc);

    [DllImport("gdi32.dll")]
    internal static extern bool BitBlt(
        IntPtr destination,
        int xDestination,
        int yDestination,
        int width,
        int height,
        IntPtr source,
        int xSource,
        int ySource,
        uint operation);

    internal static void SendMessage(
        IntPtr hwnd,
        uint message,
        nuint wParam,
        nint lParam)
    {
        SendMessageNative(
            hwnd,
            message,
            wParam,
            lParam);
    }
}
