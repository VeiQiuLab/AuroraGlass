using Microsoft.UI.Xaml;

namespace P8FreshWinUI;

internal sealed class FreshApplication : Application
{
    private readonly Action<int> finish;
    private FreshWindow? window;

    internal FreshApplication(Action<int> finish)
    {
        this.finish = finish;
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        window = new FreshWindow(code =>
        {
            finish(code);
            window = null;
        });

        window.Activate();
    }
}