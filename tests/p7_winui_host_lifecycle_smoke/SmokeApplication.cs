using AuroraGlass.WinUI;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using System.Runtime.InteropServices;
using WinRT.Interop;

namespace P7.WinUIHostLifecycleSmoke;

internal sealed class SmokeApplication : Application
{
    private readonly Action<int> setExitCode;

    private Window? window;
    private WinUIHostAttachment? attachment;
    private nint hwnd;

    private int checks;
    private int failures;

    internal SmokeApplication(
        Action<int> setExitCode)
    {
        this.setExitCode =
            setExitCode;
    }

    protected override void OnLaunched(
        LaunchActivatedEventArgs args)
    {
        try
        {
            window =
                new Window();

            window.Title =
                "AuroraGlass P7 WinUI 3 Smoke";

            window.Activate();

            hwnd =
                WindowNative.GetWindowHandle(
                    window);

            Check(
                hwnd != 0,
                "real WinUI 3 HWND obtained");

            Check(
                IsWindow(hwnd),
                "WinUI 3 HWND is a live native window");

            attachment =
                new WinUIHostAttachment();

            attachment.Attach(
                window);

            Check(
                attachment.WindowHandle == hwnd,
                "adapter receives the exact WinUI HWND");

            Check(
                attachment.IsAttached,
                "AuroraGlass native attachment is valid");

            window.Closed +=
                OnWindowClosed;

            DispatcherQueue queue =
                DispatcherQueue.GetForCurrentThread();

            if (!queue.TryEnqueue(
                    () => window.Close()))
            {
                throw new InvalidOperationException(
                    "Failed to enqueue Window.Close.");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine(
                "[FAIL] " +
                ex);

            Environment.Exit(1);
        }
    }

    private void OnWindowClosed(
        object sender,
        WindowEventArgs args)
    {
        Check(
            attachment is not null &&
            !attachment.IsAttached,
            "Window close leaves AuroraGlass detached");

        attachment?.Dispose();

        Check(
            attachment is not null &&
            !attachment.IsAttached,
            "adapter dispose after Window close is clean");

        Console.WriteLine(
            "P7_WINUI3_SMOKE: " +
            checks +
            " checks, " +
            failures +
            " failures");

        setExitCode(
            failures == 0
                ? 0
                : 1);
    }

    private void Check(
        bool condition,
        string name)
    {
        ++checks;

        if (condition)
        {
            Console.WriteLine(
                "[PASS] " +
                name);

            return;
        }

        ++failures;

        Console.WriteLine(
            "[FAIL] " +
            name);
    }

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool IsWindow(
        nint hwnd);
}