using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using WinRT;

namespace P7.WinUIHostLifecycleSmoke;

internal static class Program
{
    private static SmokeApplication? application;

    [STAThread]
    public static int Main()
    {
        int exitCode =
            1;

        ComWrappersSupport.InitializeComWrappers();

        Application.Start(
            initializationParams =>
            {
                DispatcherQueue queue =
                    DispatcherQueue.GetForCurrentThread();

                SynchronizationContext.SetSynchronizationContext(
                    new DispatcherQueueSynchronizationContext(
                        queue));

                application =
                    new SmokeApplication(
                        code => exitCode = code);
            });

        application =
            null;

        return exitCode;
    }
}