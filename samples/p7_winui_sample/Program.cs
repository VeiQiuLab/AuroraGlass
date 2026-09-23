using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using WinRT;

namespace P7.WinUISample;

internal static class Program
{
    private static SampleApplication? application;

    [STAThread]
    public static int Main(
        string[] args)
    {
        bool scripted =
            args.Contains(
                "--scripted",
                StringComparer.OrdinalIgnoreCase);

        bool captureHold =
            args.Contains(
                "--capture-hold",
                StringComparer.OrdinalIgnoreCase);

        int exitCode =
            scripted
                ? 1
                : 0;

        ComWrappersSupport.InitializeComWrappers();

        Application.Start(
            _ =>
            {
                DispatcherQueue queue =
                    DispatcherQueue.GetForCurrentThread();

                SynchronizationContext.SetSynchronizationContext(
                    new DispatcherQueueSynchronizationContext(
                        queue));

                application =
                    new SampleApplication(
                        scripted,
                        captureHold,
                        code => exitCode = code);
            });

        application = null;

        return exitCode;
    }
}