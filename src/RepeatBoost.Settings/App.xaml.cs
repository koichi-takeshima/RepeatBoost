using Microsoft.UI.Xaml;
using Microsoft.Windows.Globalization;
using System.Globalization;

namespace RepeatBoost.Settings;

public partial class App : Application
{
    private Window? _window;
    private SettingsSingleInstance? _singleInstance;

    public App()
    {
        ApplicationLanguages.PrimaryLanguageOverride =
            ResolveResourceLanguageTag(
                CultureInfo.CurrentCulture.Name);

        InitializeComponent();
    }

    internal static string ResolveResourceLanguageTag(
        string localeName)
    {
        ArgumentNullException.ThrowIfNull(localeName);

        return
            string.Equals(
                localeName,
                "ja",
                StringComparison.OrdinalIgnoreCase) ||
            localeName.StartsWith(
                "ja-",
                StringComparison.OrdinalIgnoreCase)
                ? "ja-JP"
                : "en-US";
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        var instance = SettingsSingleInstance.TryAcquire();
        if (instance is null)
        {
            Exit();
            return;
        }

        _singleInstance = instance;

        try
        {
            _window = new MainWindow();
            _window.Closed += (_, _) =>
            {
                _singleInstance?.Dispose();
                _singleInstance = null;
            };
            _window.Activate();
        }
        catch
        {
            instance.Dispose();
            _singleInstance = null;
            throw;
        }
    }
}
