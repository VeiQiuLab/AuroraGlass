using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using System.Windows.Threading;
using AuroraGlass.Wpf;

namespace P6.WpfHostLifecycle.Smoke;

internal static class Program
{
    private static int s_checks;
    private static int s_failures;

    [STAThread]
    private static int Main()
    {
        Console.WriteLine(
            "P6_WPF_HOST_LIFECYCLE_SMOKE_BEGIN");

        Application application =
            new()
            {
                ShutdownMode =
                    ShutdownMode.OnExplicitShutdown
            };

        application.Startup +=
            (_, _) =>
            {
                application.Dispatcher.BeginInvoke(
                    DispatcherPriority.ApplicationIdle,
                    new Action(
                        () => RunScenario(application)));
            };

        application.Run();

        Console.WriteLine(
            "P6_WPF_HOST_LIFECYCLE_SMOKE: " +
            s_checks +
            " checks, " +
            s_failures +
            " failures");

        return s_failures == 0 ? 0 : 1;
    }

    private static void RunScenario(
        Application application)
    {
        try
        {
            ScenarioWindowClosesFirst();
            ScenarioAdapterDisposesFirst();
        }
        catch (Exception ex)
        {
            ++s_failures;
            Console.WriteLine(
                "[FAIL] unhandled exception: " + ex);
        }
        finally
        {
            application.Shutdown(
                s_failures == 0 ? 0 : 1);
        }
    }

    private static void ScenarioWindowClosesFirst()
    {
        UserControl control = new();

        Window window =
            new()
            {
                Title = "AuroraGlass P6 WPF Host Lifecycle",
                Width = 320,
                Height = 180,
                ShowInTaskbar = false,
                ShowActivated = false,
                Content = control
            };

        bool sourceInitialized = false;

        window.SourceInitialized +=
            (_, _) =>
            {
                sourceInitialized = true;
            };

        using WpfHostAttachment attachment =
            new();

        Check(
            !attachment.Attach(window),
            "Attach before HWND does not pretend success");

        Check(
            !attachment.IsAttached,
            "detached before SourceInitialized");

        Window otherWindow = new();

        Check(
            !attachment.Attach(otherWindow),
            "different Window rejected while bound");

        Check(
            attachment.LastStatus ==
                WpfHostStatus.DifferentWindowAlreadyAttached,
            "different Window status explicit");

        window.Show();

        Check(
            sourceInitialized,
            "Window.SourceInitialized fired");

        IntPtr hwnd =
            new WindowInteropHelper(window).Handle;

        Check(
            hwnd != IntPtr.Zero,
            "WPF Window produced HWND");

        Check(
            NativeWindow.IsWindow(hwnd),
            "WPF HWND live");

        Check(
            attachment.IsAttached,
            "AuroraGlass attached after SourceInitialized");

        Check(
            attachment.Attach(window),
            "repeated same Window attach succeeds");

        Check(
            ReferenceEquals(
                Window.GetWindow(control),
                window),
            "UserControl resolves containing Window");

        window.Close();

        Check(
            !attachment.IsAttached,
            "Window close detaches AuroraGlass");

        Check(
            !NativeWindow.IsWindow(hwnd),
            "WPF destroys its own HWND");
    }

    private static void ScenarioAdapterDisposesFirst()
    {
        Window window =
            new()
            {
                Title = "AuroraGlass P6 Dispose First",
                Width = 300,
                Height = 160,
                ShowInTaskbar = false,
                ShowActivated = false
            };

        WpfHostAttachment attachment =
            new();

        Check(
            !attachment.Attach(window),
            "second scenario waits for HWND");

        window.Show();

        IntPtr hwnd =
            new WindowInteropHelper(window).Handle;

        Check(
            hwnd != IntPtr.Zero,
            "second WPF HWND exists");

        Check(
            attachment.IsAttached,
            "second Window attached");

        attachment.Dispose();

        Check(
            !attachment.IsAttached,
            "disposed adapter reports detached");

        Check(
            NativeWindow.IsWindow(hwnd),
            "disposing adapter leaves HWND alive");

        Check(
            window.IsVisible,
            "disposing adapter leaves Window alive");

        window.Close();

        Check(
            !NativeWindow.IsWindow(hwnd),
            "WPF later destroys remaining HWND");

        attachment.Dispose();
    }

    private static void Check(
        bool condition,
        string name)
    {
        ++s_checks;

        if (condition)
        {
            Console.WriteLine("[PASS] " + name);
            return;
        }

        ++s_failures;
        Console.WriteLine("[FAIL] " + name);
    }

    private static class NativeWindow
    {
        [DllImport(
            "user32.dll",
            EntryPoint = "IsWindow")]
        private static extern bool IsWindowNative(
            IntPtr hwnd);

        internal static bool IsWindow(IntPtr hwnd)
        {
            return hwnd != IntPtr.Zero &&
                IsWindowNative(hwnd);
        }
    }
}
