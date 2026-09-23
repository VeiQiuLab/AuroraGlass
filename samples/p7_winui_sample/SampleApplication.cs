using Microsoft.UI.Xaml;

namespace P7.WinUISample;

internal sealed class SampleApplication : Application
{
    private readonly bool scripted;
    private readonly bool captureHold;
    private readonly Action<int> finish;

    private SampleWindow? sampleWindow;

    internal SampleApplication(
        bool scripted,
        bool captureHold,
        Action<int> finish)
    {
        this.scripted =
            scripted;

        this.captureHold =
            captureHold;

        this.finish =
            finish;
    }

    protected override void OnLaunched(
        LaunchActivatedEventArgs args)
    {
        sampleWindow =
            new SampleWindow(
                scripted,
                captureHold,
                Finish);

        sampleWindow.Activate();
    }

    private void Finish(
        int code)
    {
        finish(
            code);

        sampleWindow =
            null;
    }
}