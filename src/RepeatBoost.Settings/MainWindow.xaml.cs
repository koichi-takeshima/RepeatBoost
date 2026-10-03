using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.Windows.ApplicationModel.Resources;
using System.Globalization;
using System.Runtime.InteropServices;
using Windows.Foundation;
using Windows.Graphics;

namespace RepeatBoost.Settings;

public sealed partial class MainWindow : Window
{
    private const int DwmwaCloak = 13;

    private readonly string _settingsPath;
    private readonly AutostartRegistration _autostart;
    private readonly ResourceLoader _resources = new();

    private XamlRoot? _xamlRoot;
    private string? _startupWarning;
    private bool _closeWithoutSaving;
    private bool _saveFailureDialogOpen;

    // ExtractIconExW transfers ownership of these HICONs to this Window.
    // AppWindow's icon ID may reference the underlying HICON, so keep the
    // handles alive for the Window lifetime and release them from Closed.
    private nint _windowLargeIcon;
    private nint _windowSmallIcon;

    private Settings _originalSettings =
        Settings.Defaults;
    private AutostartState _originalAutostart =
        AutostartState.Disabled;

    public MainWindow()
    {
        InitializeComponent();
        SetStartupCloak(true);

        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);

        ApplyExecutableIcon();

        if (Content is FrameworkElement contentRoot)
        {
            contentRoot.Loaded += ContentRoot_Loaded;
        }

        AppWindow.Closing += AppWindow_Closing;
        Closed += MainWindow_Closed;

        _settingsPath =
            SettingsPaths.GetDefaultSettingsFilePath();

        _autostart = new AutostartRegistration(
            new CurrentUserRunKeyAccess(),
            SettingsPaths.GetEngineExecutablePath());

        LoadCurrentState();
    }

    private async void ContentRoot_Loaded(
        object sender,
        RoutedEventArgs e)
    {
        var contentRoot = (FrameworkElement)sender;
        contentRoot.Loaded -= ContentRoot_Loaded;

        _xamlRoot = contentRoot.XamlRoot;
        if (_xamlRoot is null)
        {
            throw new InvalidOperationException(
                "Settings XamlRoot is unavailable after content load.");
        }

        _xamlRoot.Changed += XamlRoot_Changed;
        ConfigureFixedWindow();
        SetStartupCloak(false);

        if (!string.IsNullOrWhiteSpace(_startupWarning))
        {
            var dialog = new ContentDialog
            {
                Title = _resources.GetString(
                    "StartupWarningTitle"),
                Content = _startupWarning,
                CloseButtonText = _resources.GetString(
                    "OkButton"),
                XamlRoot = _xamlRoot,
            };

            _startupWarning = null;
            await dialog.ShowAsync();
        }
    }

    private async void AppWindow_Closing(
        AppWindow sender,
        AppWindowClosingEventArgs args)
    {
        if (_closeWithoutSaving)
        {
            return;
        }

        if (_saveFailureDialogOpen)
        {
            args.Cancel = true;
            return;
        }

        if (TrySaveCurrentState(out var error))
        {
            return;
        }

        args.Cancel = true;

        if (_xamlRoot is null)
        {
            return;
        }

        _saveFailureDialogOpen = true;

        try
        {
            while (true)
            {
                var dialog = new ContentDialog
                {
                    Title = _resources.GetString(
                        "SettingsNotSavedTitle"),
                    Content =
                        error ??
                        _resources.GetString(
                            "SettingsNotSavedFallback"),
                    PrimaryButtonText = _resources.GetString(
                        "RetryButton"),
                    SecondaryButtonText = _resources.GetString(
                        "DiscardAndCloseButton"),
                    CloseButtonText = _resources.GetString(
                        "CancelButton"),
                    DefaultButton = ContentDialogButton.Primary,
                    XamlRoot = _xamlRoot,
                };

                var result = await dialog.ShowAsync();

                if (result == ContentDialogResult.Secondary)
                {
                    _closeWithoutSaving = true;
                    Close();
                    return;
                }

                if (result != ContentDialogResult.Primary)
                {
                    return;
                }

                if (TrySaveCurrentState(out error))
                {
                    _closeWithoutSaving = true;
                    Close();
                    return;
                }
            }
        }
        finally
        {
            _saveFailureDialogOpen = false;
        }
    }

    private void MainWindow_Closed(
        object sender,
        WindowEventArgs args)
    {
        AppWindow.Closing -= AppWindow_Closing;

        if (_xamlRoot is not null)
        {
            _xamlRoot.Changed -= XamlRoot_Changed;
            _xamlRoot = null;
        }

        ReleaseExecutableIcons();
    }

    private void XamlRoot_Changed(
        XamlRoot sender,
        XamlRootChangedEventArgs args) =>
        ApplyContentDesiredClientSize();

    private void ConfigureFixedWindow()
    {
        if (AppWindow.Presenter is OverlappedPresenter presenter)
        {
            presenter.IsResizable = false;
            presenter.IsMaximizable = false;
        }

        ApplyContentDesiredClientSize();
    }

    private void ApplyContentDesiredClientSize()
    {
        if (Content is not FrameworkElement contentRoot ||
            _xamlRoot is null)
        {
            return;
        }

        var rasterizationScale =
            _xamlRoot.RasterizationScale;

        if (!double.IsFinite(rasterizationScale) ||
            rasterizationScale <= 0)
        {
            return;
        }

        contentRoot.Measure(
            new Size(
                double.PositiveInfinity,
                double.PositiveInfinity));

        var desiredSize = contentRoot.DesiredSize;
        if (!double.IsFinite(desiredSize.Width) ||
            !double.IsFinite(desiredSize.Height) ||
            desiredSize.Width <= 0 ||
            desiredSize.Height <= 0)
        {
            return;
        }

        var desiredClientWidth =
            checked((int)Math.Ceiling(
                desiredSize.Width *
                rasterizationScale));

        var desiredRootHeight =
            checked((int)Math.Ceiling(
                desiredSize.Height *
                rasterizationScale));

        var desiredClientHeight =
            checked(
                desiredRootHeight -
                AppWindow.TitleBar.Height);

        if (desiredClientHeight <= 0)
        {
            return;
        }

        AppWindow.ResizeClient(
            new SizeInt32(
                desiredClientWidth,
                desiredClientHeight));
    }


    private void SetStartupCloak(bool cloaked)
    {
        var value = cloaked ? 1 : 0;
        var result = DwmSetWindowAttribute(
            WinRT.Interop.WindowNative.GetWindowHandle(this),
            DwmwaCloak,
            ref value,
            sizeof(int));

        Marshal.ThrowExceptionForHR(result);
    }

    [DllImport("dwmapi.dll")]
    private static extern int DwmSetWindowAttribute(
        nint window,
        int attribute,
        ref int value,
        int valueSize);

    private void ApplyExecutableIcon()
    {
        var executablePath = Environment.ProcessPath;
        if (string.IsNullOrWhiteSpace(executablePath))
        {
            return;
        }

        var iconCount = ExtractIconEx(
            executablePath,
            0,
            out _windowLargeIcon,
            out _windowSmallIcon,
            1);

        if (iconCount == 0)
        {
            ReleaseExecutableIcons();
            return;
        }

        var icon = _windowSmallIcon != nint.Zero
            ? _windowSmallIcon
            : _windowLargeIcon;

        if (icon == nint.Zero)
        {
            ReleaseExecutableIcons();
            return;
        }

        try
        {
            AppWindow.SetIcon(
                Win32Interop.GetIconIdFromIcon(icon));
        }
        catch
        {
            ReleaseExecutableIcons();
            throw;
        }
    }

    private void ReleaseExecutableIcons()
    {
        if (_windowSmallIcon != nint.Zero)
        {
            DestroyIcon(_windowSmallIcon);
            _windowSmallIcon = nint.Zero;
        }

        if (_windowLargeIcon != nint.Zero)
        {
            DestroyIcon(_windowLargeIcon);
            _windowLargeIcon = nint.Zero;
        }
    }

    [DllImport(
        "shell32.dll",
        CharSet = CharSet.Unicode,
        EntryPoint = "ExtractIconExW")]
    private static extern uint ExtractIconEx(
        string file,
        int iconIndex,
        out nint largeIcon,
        out nint smallIcon,
        uint iconCount);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DestroyIcon(nint icon);

    private void LoadCurrentState()
    {
        var settings = Settings.Load(_settingsPath);
        _originalSettings = settings;

        EnabledToggle.IsOn = settings.Enabled;
        InitialDelayBox.Value = settings.InitialDelayMs;
        RepeatIntervalBox.Value = settings.RepeatIntervalMs;
        TargetPresetBox.SelectedIndex =
            settings.TargetPreset == TargetPreset.AllKeys ? 1 : 0;

        _startupWarning =
            ApplyAutostartResult(_autostart.Query());
    }

    private bool TrySaveCurrentState(
        out string? error)
    {
        try
        {
            var desired = ReadSettingsFromControls();
            var patch = BuildPatch(
                _originalSettings,
                desired);

            _originalSettings =
                Settings.Patch(_settingsPath, patch);

            var requestedAutostart = AutostartToggle.IsOn;
            if (_originalAutostart is
                    AutostartState.Enabled or
                    AutostartState.Disabled)
            {
                var currentlyEnabled =
                    _originalAutostart == AutostartState.Enabled;

                var result =
                    requestedAutostart != currentlyEnabled
                        ? _autostart.SetEnabled(
                            requestedAutostart)
                        : _autostart.Query();

                var expectedState =
                    requestedAutostart
                        ? AutostartState.Enabled
                        : AutostartState.Disabled;

                if (result.State != expectedState)
                {
                    error = result.State switch
                    {
                        AutostartState.Conflict =>
                            _resources.GetString(
                                "AutostartConflict"),
                        AutostartState.Error
                            when !string.IsNullOrWhiteSpace(
                                result.Message) =>
                            $"{_resources.GetString(
                                "AutostartSettingNotSaved")} " +
                            result.Message,
                        _ => _resources.GetString(
                            "AutostartSettingNotSaved"),
                    };
                    return false;
                }

                ApplyAutostartResult(result);
            }

            error = null;
            return true;
        }
        catch (Exception ex)
        {
            error =
                $"{_resources.GetString(
                    "SettingsNotSavedPrefix")} {ex.Message}";
            return false;
        }
    }

    private Settings ReadSettingsFromControls()
    {
        var initialDelay = ReadWholeNumber(
            InitialDelayBox.Value,
            Settings.InitialDelayMinMs,
            Settings.InitialDelayMaxMs,
            nameof(Settings.InitialDelayMs));

        var repeatInterval = ReadWholeNumber(
            RepeatIntervalBox.Value,
            Settings.RepeatIntervalMinMs,
            Settings.RepeatIntervalMaxMs,
            nameof(Settings.RepeatIntervalMs));

        var targetPreset = TargetPresetBox.SelectedIndex switch
        {
            0 => TargetPreset.ArrowKeys,
            1 => TargetPreset.AllKeys,
            _ => throw new InvalidOperationException(
                _resources.GetString(
                    "SelectTargetPreset")),
        };

        return new Settings(
            Settings.CurrentSchemaVersion,
            EnabledToggle.IsOn,
            initialDelay,
            repeatInterval,
            targetPreset);
    }

    private static SettingsPatch BuildPatch(
        Settings original,
        Settings desired) =>
        new(
            Enabled:
                original.Enabled != desired.Enabled
                    ? desired.Enabled
                    : null,
            InitialDelayMs:
                original.InitialDelayMs != desired.InitialDelayMs
                    ? desired.InitialDelayMs
                    : null,
            RepeatIntervalMs:
                original.RepeatIntervalMs != desired.RepeatIntervalMs
                    ? desired.RepeatIntervalMs
                    : null,
            TargetPreset:
                original.TargetPreset != desired.TargetPreset
                    ? desired.TargetPreset
                    : null);

    private int ReadWholeNumber(
        double value,
        int minimum,
        int maximum,
        string name)
    {
        if (double.IsNaN(value) ||
            value != Math.Truncate(value) ||
            value < minimum ||
            value > maximum)
        {
            throw new ArgumentOutOfRangeException(
                name,
                string.Format(
                    CultureInfo.CurrentCulture,
                    _resources.GetString(
                        "WholeNumberRange"),
                    minimum,
                    maximum));
        }

        return checked((int)value);
    }

    private string? ApplyAutostartResult(
        AutostartResult result)
    {
        _originalAutostart = result.State;

        switch (result.State)
        {
            case AutostartState.Enabled:
                AutostartToggle.IsEnabled = true;
                AutostartToggle.IsOn = true;
                return null;

            case AutostartState.Disabled:
                AutostartToggle.IsEnabled = true;
                AutostartToggle.IsOn = false;
                return null;

            case AutostartState.Conflict:
                AutostartToggle.IsEnabled = false;
                return _resources.GetString(
                    "AutostartConflict");

            default:
                AutostartToggle.IsEnabled = false;
                var message = _resources.GetString(
                    "AutostartReadFailed");
                return string.IsNullOrWhiteSpace(result.Message)
                    ? message
                    : $"{message} {result.Message}";
        }
    }
}
