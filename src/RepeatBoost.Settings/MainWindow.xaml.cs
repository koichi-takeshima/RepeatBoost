using CommunityToolkit.WinUI.Controls;
using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.Windows.ApplicationModel.Resources;
using System.Collections.ObjectModel;
using System.Globalization;
using System.Runtime.InteropServices;
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
    private CustomKeyCaptureState _customKeyCapture;
    private readonly ObservableCollection<RepeatSetEditorItem>
        _repeatSetEditors = [];
    private RepeatSetEditorItem? _customKeyCaptureOwner;
    private Button? _customKeyCaptureButton;
    private ContentControl? _customKeyCaptureSurface;
    private bool _enabledEdited;
    private RectInt32? _normalWindowBounds;
    private bool _windowMaximizedCandidate;
    private bool _restoreMaximizedPending;

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

        _settingsPath =
            SettingsPaths.GetDefaultSettingsFilePath();

        _autostart = new AutostartRegistration(
            new CurrentUserRunKeyAccess(),
            SettingsPaths.GetEngineExecutablePath());

        RestoreWindowPlacement();

        if (Content is FrameworkElement contentRoot)
        {
            contentRoot.Loaded += ContentRoot_Loaded;
        }

        AppWindow.Closing += AppWindow_Closing;
        Closed += MainWindow_Closed;

        RepeatSetItems.ItemsSource =
            _repeatSetEditors;

        LoadCurrentState();
        EnabledToggle.Toggled +=
            EnabledToggle_Toggled;
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

        ApplyPendingMaximizedRestore();
        InitializeWindowPlacementTracking();
        AppWindow.Changed += AppWindow_Changed;

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
        AppWindow.Changed -= AppWindow_Changed;
        AppWindow.Closing -= AppWindow_Closing;

        SaveWindowPlacementBestEffort();

        _xamlRoot = null;

        ReleaseExecutableIcons();
    }

    private void RestoreWindowPlacement()
    {
        try
        {
            if (SettingsWindowPlacementStore.TryLoad(
                    _settingsPath,
                    out var placement))
            {
                var displayArea =
                    DisplayArea.GetFromRect(
                        placement.NormalBounds,
                        DisplayAreaFallback.Nearest);
                var correctedBounds =
                    SettingsWindowPlacementStore
                        .ConstrainToWorkArea(
                            placement.NormalBounds,
                            displayArea.WorkArea);

                AppWindow.MoveAndResize(
                    correctedBounds);
                _normalWindowBounds =
                    correctedBounds;
                _windowMaximizedCandidate =
                    placement.Maximized;
                _restoreMaximizedPending =
                    placement.Maximized;

                return;
            }
        }
        catch
        {
            // Window placement is convenience state. Fall back to the
            // platform default without making Settings startup fail.
        }

        InitializeWindowPlacementTracking();
    }

    private void ApplyPendingMaximizedRestore()
    {
        if (!_restoreMaximizedPending)
        {
            return;
        }

        _restoreMaximizedPending = false;

        try
        {
            if (AppWindow.Presenter is
                OverlappedPresenter presenter)
            {
                presenter.Maximize();
                _windowMaximizedCandidate =
                    true;
                return;
            }
        }
        catch
        {
            // Normal placement remains valid if maximized restore fails.
        }

        _windowMaximizedCandidate =
            false;
    }

    private void InitializeWindowPlacementTracking()
    {
        if (AppWindow.Presenter is not
            OverlappedPresenter presenter)
        {
            return;
        }

        SettingsWindowPlacementStore
            .UpdateTracking(
                presenter.State,
                CurrentWindowBounds(),
                ref _normalWindowBounds,
                ref _windowMaximizedCandidate);
    }

    private void AppWindow_Changed(
        AppWindow sender,
        AppWindowChangedEventArgs args)
    {
        if (sender.Presenter is not
            OverlappedPresenter presenter)
        {
            return;
        }

        SettingsWindowPlacementStore
            .UpdateTracking(
                presenter.State,
                CurrentWindowBounds(),
                ref _normalWindowBounds,
                ref _windowMaximizedCandidate);
    }

    private RectInt32 CurrentWindowBounds()
    {
        var position =
            AppWindow.Position;
        var size =
            AppWindow.Size;

        return new RectInt32(
            position.X,
            position.Y,
            size.Width,
            size.Height);
    }

    private void SaveWindowPlacementBestEffort()
    {
        if (_normalWindowBounds is not
            RectInt32 normalBounds)
        {
            return;
        }

        try
        {
            SettingsWindowPlacementStore.Save(
                _settingsPath,
                new SettingsWindowPlacement(
                    normalBounds,
                    _windowMaximizedCandidate));
        }
        catch
        {
            // Placement persistence is independent convenience state.
            // It must not turn a successful Settings close into a failure.
        }
    }

    private void RepeatSetExpander_Collapsed(
        object? sender,
        EventArgs e)
    {
        if (sender is SettingsExpander expander)
        {
            expander.IsExpanded = true;
        }
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

        _repeatSetEditors.Clear();
        foreach (var setting in settings.RepeatSets)
        {
            _repeatSetEditors.Add(
                new RepeatSetEditorItem(
                    setting));
        }

        if (_repeatSetEditors.Count == 0)
        {
            _repeatSetEditors.Add(
                new RepeatSetEditorItem(
                    RepeatSettingSet.Default));
        }

        RefreshRepeatSetEditorPositions();

        _startupWarning =
            ApplyAutostartResult(_autostart.Query());
    }

    private bool TrySaveCurrentState(
        out string? error)
    {
        try
        {
            var desired = ReadSettingsFromControls();

            _originalSettings =
                Settings.Save(
                    _settingsPath,
                    desired,
                    enabledEdited:
                        _enabledEdited);
            _enabledEdited = false;

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

    private void EnabledToggle_Toggled(
        object sender,
        RoutedEventArgs e)
    {
        _enabledEdited = true;
    }

    private Settings ReadSettingsFromControls()
    {
        CancelCustomKeyCapture();

        var repeatSets =
            _repeatSetEditors
                .Select(ReadRepeatSettingSet)
                .ToArray();

        return new Settings(
            EnabledToggle.IsOn,
            repeatSets);
    }

    private RepeatSettingSet ReadRepeatSettingSet(
        RepeatSetEditorItem item)
    {
        var initialDelay = ReadWholeNumber(
            item.InitialDelayMs,
            Settings.InitialDelayMinMs,
            Settings.InitialDelayMaxMs,
            nameof(RepeatSettingSet.InitialDelayMs));

        var repeatInterval = ReadWholeNumber(
            item.RepeatIntervalMs,
            Settings.RepeatIntervalMinMs,
            Settings.RepeatIntervalMaxMs,
            nameof(RepeatSettingSet.RepeatIntervalMs));

        var targetPreset =
            item.TargetPresetIndex switch
            {
                0 => TargetPreset.ArrowKeys,
                1 => TargetPreset.AllKeys,
                2 => TargetPreset.Custom,
                _ => throw new InvalidOperationException(
                    _resources.GetString(
                        "SelectTargetPreset")),
            };

        return new RepeatSettingSet(
            initialDelay,
            repeatInterval,
            targetPreset,
            item.CustomTargets);
    }

    private void RefreshRepeatSetEditorPositions()
    {
        var count =
            _repeatSetEditors.Count;

        for (var index = 0;
             index < count;
             ++index)
        {
            _repeatSetEditors[index]
                .UpdatePosition(
                    string.Format(
                        CultureInfo.CurrentCulture,
                        _resources.GetString(
                            "RepeatSetDisplayName"),
                        index + 1),
                    canMoveUp:
                        index > 0,
                    canMoveDown:
                        index < count - 1,
                    canDelete:
                        count >
                        Settings.MinRepeatSetCount);
        }

        AddRepeatSetButton.IsEnabled =
            count < Settings.MaxRepeatSetCount;
    }

    private static RepeatSetEditorItem?
        EditorItemFromSender(
            object sender) =>
        (sender as FrameworkElement)?
            .Tag as RepeatSetEditorItem;

    private void AddRepeatSetButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        if (_repeatSetEditors.Count >=
            Settings.MaxRepeatSetCount)
        {
            return;
        }

        CancelCustomKeyCapture();

        _repeatSetEditors.Add(
            new RepeatSetEditorItem(
                RepeatSettingSet.Default));

        RefreshRepeatSetEditorPositions();
    }

    private void DeleteRepeatSetButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        var item =
            EditorItemFromSender(sender);
        if (item is null ||
            _repeatSetEditors.Count <=
                Settings.MinRepeatSetCount)
        {
            return;
        }

        CancelCustomKeyCapture();

        var index =
            _repeatSetEditors.IndexOf(item);
        if (index < 0)
        {
            return;
        }

        _repeatSetEditors.RemoveAt(index);
        RefreshRepeatSetEditorPositions();
    }

    private void MoveRepeatSetUpButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        MoveRepeatSet(
            EditorItemFromSender(sender),
            -1);
    }

    private void MoveRepeatSetDownButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        MoveRepeatSet(
            EditorItemFromSender(sender),
            1);
    }

    private void MoveRepeatSet(
        RepeatSetEditorItem? item,
        int delta)
    {
        if (item is null)
        {
            return;
        }

        var source =
            _repeatSetEditors.IndexOf(item);
        var destination =
            source + delta;

        if (source < 0 ||
            destination < 0 ||
            destination >=
                _repeatSetEditors.Count)
        {
            return;
        }

        CancelCustomKeyCapture();

        _repeatSetEditors.Move(
            source,
            destination);

        RefreshRepeatSetEditorPositions();
    }

    private void TargetPresetBox_SelectionChanged(
        object sender,
        SelectionChangedEventArgs e)
    {
        if (sender is not ComboBox comboBox ||
            comboBox.Tag is not
                RepeatSetEditorItem item)
        {
            return;
        }

        item.TargetPresetIndex =
            comboBox.SelectedIndex;

        if (item.TargetPresetIndex != 2 &&
            ReferenceEquals(
                item,
                _customKeyCaptureOwner))
        {
            CancelCustomKeyCapture();
        }

    }

    private void AddCustomKeyButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        if (sender is not Button button ||
            button.Tag is not
                RepeatSetEditorItem item ||
            button.Parent is not
                StackPanel actionRow)
        {
            return;
        }

        var captureSurface =
            actionRow.Children
                .OfType<ContentControl>()
                .SingleOrDefault(
                    control =>
                        control is not Button);

        if (captureSurface is null)
        {
            return;
        }

        CancelCustomKeyCapture();

        _customKeyCapture.Start();
        _customKeyCaptureOwner = item;
        _customKeyCaptureButton = button;
        _customKeyCaptureSurface =
            captureSurface;

        button.Visibility =
            Visibility.Collapsed;
        captureSurface.Content =
            _resources.GetString(
                "PressCustomKey");
        captureSurface.Visibility =
            Visibility.Visible;

        if (!captureSurface.Focus(
                FocusState.Programmatic))
        {
            CancelCustomKeyCapture();
        }
    }

    private void CustomKeyCaptureSurface_KeyDown(
        object sender,
        KeyRoutedEventArgs e)
    {
        if (sender is not ContentControl surface ||
            surface.Tag is not
                RepeatSetEditorItem item ||
            !ReferenceEquals(
                item,
                _customKeyCaptureOwner))
        {
            return;
        }

        e.Handled = true;
        var virtualKey =
            (int)e.Key;

        if (!_customKeyCapture.TryAccept(
                virtualKey))
        {
            return;
        }

        item.AddCustomTarget(virtualKey);
        CompleteCustomKeyCaptureUi();

    }

    private void CustomKeyCaptureSurface_LostFocus(
        object sender,
        RoutedEventArgs e)
    {
        if (_customKeyCapture.IsActive &&
            ReferenceEquals(
                sender,
                _customKeyCaptureSurface))
        {
            CancelCustomKeyCapture();
        }
    }

    private void CustomKeyTokens_TextChanged(
        AutoSuggestBox sender,
        AutoSuggestBoxTextChangedEventArgs e)
    {
        if (!string.IsNullOrEmpty(
                sender.Text))
        {
            sender.Text =
                string.Empty;
        }
    }

    private void CustomKeyTokens_TokenItemAdding(
        TokenizingTextBox sender,
        TokenItemAddingEventArgs e) =>
        e.Cancel = true;

    private void CustomKeyTokens_TokenItemRemoved(
        TokenizingTextBox sender,
        object item)
    {
        if (sender.Tag is not
                RepeatSetEditorItem editor ||
            item is not CustomKeyToken token)
        {
            return;
        }

        editor.RemoveCustomTarget(
            token.VirtualKey);

    }

    private void CancelCustomKeyCapture()
    {
        _customKeyCapture.Cancel();
        CompleteCustomKeyCaptureUi();
    }

    private void CompleteCustomKeyCaptureUi()
    {
        if (_customKeyCaptureSurface is not null)
        {
            _customKeyCaptureSurface.Visibility =
                Visibility.Collapsed;
            _customKeyCaptureSurface.Content =
                null;
        }

        if (_customKeyCaptureButton is not null)
        {
            _customKeyCaptureButton.Visibility =
                Visibility.Visible;
        }

        _customKeyCaptureOwner = null;
        _customKeyCaptureButton = null;
        _customKeyCaptureSurface = null;
    }

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
