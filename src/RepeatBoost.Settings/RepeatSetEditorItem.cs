using Microsoft.UI.Xaml;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace RepeatBoost.Settings;

internal sealed class RepeatSetEditorItem : INotifyPropertyChanged
{
    private double _initialDelayMs;
    private double _repeatIntervalMs;
    private int _targetPresetIndex;
    private CustomTargetSet _customTargets;
    private string _displayName = string.Empty;
    private bool _canMoveUp;
    private bool _canMoveDown;
    private bool _canDelete;

    public RepeatSetEditorItem(
        RepeatSettingSet setting)
    {
        _initialDelayMs =
            setting.InitialDelayMs;
        _repeatIntervalMs =
            setting.RepeatIntervalMs;
        _targetPresetIndex =
            setting.TargetPreset switch
            {
                TargetPreset.ArrowKeys => 0,
                TargetPreset.AllKeys => 1,
                TargetPreset.Custom => 2,
                _ => 0,
            };
        _customTargets =
            setting.CustomTargets;

        RebuildCustomKeyTokens();
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    // Typed x:Bind owns the per-set template state. Self lets event-source
    // Tag preserve that same item identity across nested SettingsExpander cards
    // without creating a selected-item or duplicate UI collection.
    public RepeatSetEditorItem Self => this;

    public double InitialDelayMs
    {
        get => _initialDelayMs;
        set => SetField(
            ref _initialDelayMs,
            value);
    }

    public double RepeatIntervalMs
    {
        get => _repeatIntervalMs;
        set => SetField(
            ref _repeatIntervalMs,
            value);
    }

    public int TargetPresetIndex
    {
        get => _targetPresetIndex;
        set
        {
            if (!SetField(
                    ref _targetPresetIndex,
                    value))
            {
                return;
            }

            OnPropertyChanged(
                nameof(CustomEditorVisibility));
        }
    }

    public CustomTargetSet CustomTargets =>
        _customTargets;

    public ObservableCollection<CustomKeyToken>
        CustomKeyTokens { get; } = [];

    public string DisplayName
    {
        get => _displayName;
        private set => SetField(
            ref _displayName,
            value);
    }

    public bool CanMoveUp
    {
        get => _canMoveUp;
        private set => SetField(
            ref _canMoveUp,
            value);
    }

    public bool CanMoveDown
    {
        get => _canMoveDown;
        private set => SetField(
            ref _canMoveDown,
            value);
    }

    public bool CanDelete
    {
        get => _canDelete;
        private set => SetField(
            ref _canDelete,
            value);
    }

    public Visibility CustomEditorVisibility =>
        TargetPresetIndex == 2
            ? Visibility.Visible
            : Visibility.Collapsed;

    public void UpdatePosition(
        string displayName,
        bool canMoveUp,
        bool canMoveDown,
        bool canDelete)
    {
        DisplayName = displayName;
        CanMoveUp = canMoveUp;
        CanMoveDown = canMoveDown;
        CanDelete = canDelete;
    }

    public void AddCustomTarget(
        int virtualKey)
    {
        _customTargets =
            _customTargets.Set(virtualKey);
        RebuildCustomKeyTokens();
    }

    public void RemoveCustomTarget(
        int virtualKey)
    {
        _customTargets =
            _customTargets.Set(
                virtualKey,
                included: false);
        OnPropertyChanged(
            nameof(CustomTargets));
    }

    private void RebuildCustomKeyTokens()
    {
        CustomKeyTokens.Clear();

        foreach (var virtualKey in
                 _customTargets.Enumerate())
        {
            CustomKeyTokens.Add(
                new CustomKeyToken(
                    virtualKey,
                    GetCustomKeyDisplayName(
                        virtualKey)));
        }

        OnPropertyChanged(
            nameof(CustomTargets));
    }

    private static string GetCustomKeyDisplayName(
        int virtualKey)
    {
        var key =
            (Windows.System.VirtualKey)virtualKey;
        var name =
            Enum.GetName(key);

        return string.IsNullOrEmpty(name)
            ? $"VK 0x{virtualKey:X2}"
            : $"{name} (0x{virtualKey:X2})";
    }

    private bool SetField<T>(
        ref T field,
        T value,
        [CallerMemberName]
        string? propertyName = null)
    {
        if (EqualityComparer<T>.Default.Equals(
                field,
                value))
        {
            return false;
        }

        field = value;
        OnPropertyChanged(propertyName);
        return true;
    }

    private void OnPropertyChanged(
        [CallerMemberName]
        string? propertyName = null) =>
        PropertyChanged?.Invoke(
            this,
            new PropertyChangedEventArgs(
                propertyName));
}

internal sealed record CustomKeyToken(
    int VirtualKey,
    string DisplayName);
