using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

namespace RepeatBoost.Settings;

public enum TargetPreset
{
    ArrowKeys,
    AllKeys,
    Custom,
}

public readonly record struct CustomTargetSet(
    ulong Mask0,
    ulong Mask1,
    ulong Mask2,
    ulong Mask3)
{
    public static CustomTargetSet Empty => default;

    public bool Contains(int virtualKey)
    {
        if ((uint)virtualKey > 0xFFU)
        {
            return false;
        }

        var mask = (virtualKey >> 6) switch
        {
            0 => Mask0,
            1 => Mask1,
            2 => Mask2,
            _ => Mask3,
        };
        return (mask & (1UL << (virtualKey & 0x3F))) != 0;
    }

    public CustomTargetSet Set(
        int virtualKey,
        bool included = true)
    {
        if ((uint)virtualKey > 0xFFU)
        {
            return this;
        }

        var bit = 1UL << (virtualKey & 0x3F);
        return (virtualKey >> 6) switch
        {
            0 => this with { Mask0 = included ? Mask0 | bit : Mask0 & ~bit },
            1 => this with { Mask1 = included ? Mask1 | bit : Mask1 & ~bit },
            2 => this with { Mask2 = included ? Mask2 | bit : Mask2 & ~bit },
            _ => this with { Mask3 = included ? Mask3 | bit : Mask3 & ~bit },
        };
    }

    public IEnumerable<int> Enumerate()
    {
        for (var virtualKey = 1; virtualKey <= 0xFE; ++virtualKey)
        {
            if (Contains(virtualKey))
            {
                yield return virtualKey;
            }
        }
    }

    public string ToPersistedString() =>
        Mask0.ToString("X16", System.Globalization.CultureInfo.InvariantCulture) +
        Mask1.ToString("X16", System.Globalization.CultureInfo.InvariantCulture) +
        Mask2.ToString("X16", System.Globalization.CultureInfo.InvariantCulture) +
        Mask3.ToString("X16", System.Globalization.CultureInfo.InvariantCulture);

    public static bool TryParsePersistedString(
        string? text,
        out CustomTargetSet targets)
    {
        targets = Empty;
        if (text is null || text.Length != 64)
        {
            return false;
        }

        Span<ulong> masks = stackalloc ulong[4];
        for (var index = 0; index < masks.Length; ++index)
        {
            if (!ulong.TryParse(
                    text.AsSpan(index * 16, 16),
                    System.Globalization.NumberStyles.AllowHexSpecifier,
                    System.Globalization.CultureInfo.InvariantCulture,
                    out masks[index]))
            {
                return false;
            }
        }

        targets = new CustomTargetSet(
            masks[0],
            masks[1],
            masks[2],
            masks[3]);
        return true;
    }

    public static bool IsSelectableVirtualKey(int virtualKey) =>
        virtualKey is >= 1 and <= 0xFE &&
        virtualKey is not (
            0x10 or 0x11 or 0x12 or
            0x5B or 0x5C or
            0xA0 or 0xA1 or
            0xA2 or 0xA3 or
            0xA4 or 0xA5 or
            0x14 or 0x90 or 0x91);
}

public readonly record struct RepeatSettingSet(
    int InitialDelayMs,
    int RepeatIntervalMs,
    TargetPreset TargetPreset,
    CustomTargetSet CustomTargets = default)
{
    public static RepeatSettingSet Default { get; } = new(
        Settings.InitialDelayMaxMs,
        Settings.RepeatIntervalMaxMs,
        TargetPreset.ArrowKeys,
        CustomTargetSet.Empty);
}

public sealed class Settings : IEquatable<Settings>
{
    private const string SettingsSection = "Settings";
    private const int StringBufferSize = 128;

    public const int LegacySchemaVersion = 1;
    public const int CurrentSchemaVersion = 2;
    public const int MinRepeatSetCount = 1;
    public const int MaxRepeatSetCount = 16;
    public const int InitialDelayMinMs = 0;
    public const int InitialDelayMaxMs = 250;
    public const int RepeatIntervalMinMs = 5;
    public const int RepeatIntervalMaxMs = 33;

    public Settings(
        bool enabled,
        IReadOnlyList<RepeatSettingSet> repeatSets)
    {
        ArgumentNullException.ThrowIfNull(repeatSets);
        Enabled = enabled;
        RepeatSets = repeatSets.ToArray();
    }

    public Settings(
        int schemaVersion,
        bool enabled,
        int initialDelayMs,
        int repeatIntervalMs,
        TargetPreset targetPreset,
        CustomTargetSet customTargets = default)
        : this(
            enabled,
            new[]
            {
                new RepeatSettingSet(
                    initialDelayMs,
                    repeatIntervalMs,
                    targetPreset,
                    customTargets),
            })
    {
        _ = schemaVersion;
    }

    public int SchemaVersion => CurrentSchemaVersion;
    public bool Enabled { get; }
    public IReadOnlyList<RepeatSettingSet> RepeatSets { get; }

    // Compatibility views of the highest-priority set.
    public int InitialDelayMs => RepeatSets[0].InitialDelayMs;
    public int RepeatIntervalMs => RepeatSets[0].RepeatIntervalMs;
    public TargetPreset TargetPreset => RepeatSets[0].TargetPreset;
    public CustomTargetSet CustomTargets => RepeatSets[0].CustomTargets;

    public static Settings Defaults { get; } = new(
        false,
        new[] { RepeatSettingSet.Default });

    public bool Equals(Settings? other)
    {
        if (ReferenceEquals(this, other))
        {
            return true;
        }

        if (other is null ||
            Enabled != other.Enabled ||
            RepeatSets.Count != other.RepeatSets.Count)
        {
            return false;
        }

        for (var index = 0; index < RepeatSets.Count; ++index)
        {
            if (RepeatSets[index] != other.RepeatSets[index])
            {
                return false;
            }
        }

        return true;
    }

    public override bool Equals(object? obj) =>
        obj is Settings other && Equals(other);

    public override int GetHashCode()
    {
        var hash = new HashCode();
        hash.Add(Enabled);
        foreach (var setting in RepeatSets)
        {
            hash.Add(setting);
        }

        return hash.ToHashCode();
    }

    public static bool TryValidate(
        Settings settings,
        out string? error)
    {
        ArgumentNullException.ThrowIfNull(settings);

        if (settings.RepeatSets.Count < MinRepeatSetCount ||
            settings.RepeatSets.Count > MaxRepeatSetCount)
        {
            error =
                $"RepeatSetCount out of range: {settings.RepeatSets.Count}";
            return false;
        }

        for (var index = 0;
             index < settings.RepeatSets.Count;
             ++index)
        {
            if (!TryValidateRepeatSet(
                    settings.RepeatSets[index],
                    out error))
            {
                error = $"RepeatSet{index}: {error}";
                return false;
            }
        }

        error = null;
        return true;
    }

    private static bool TryValidateRepeatSet(
        RepeatSettingSet setting,
        out string? error)
    {
        if (setting.InitialDelayMs < InitialDelayMinMs ||
            setting.InitialDelayMs > InitialDelayMaxMs)
        {
            error =
                $"InitialDelayMs out of range: {setting.InitialDelayMs}";
            return false;
        }

        if (setting.RepeatIntervalMs < RepeatIntervalMinMs ||
            setting.RepeatIntervalMs > RepeatIntervalMaxMs)
        {
            error =
                $"RepeatIntervalMs out of range: {setting.RepeatIntervalMs}";
            return false;
        }

        if (!Enum.IsDefined(setting.TargetPreset))
        {
            error =
                $"Unknown TargetPreset: {setting.TargetPreset}";
            return false;
        }

        error = null;
        return true;
    }

    public static Settings Load(string path)
    {
        path = System.IO.Path.GetFullPath(path);

        var schemaVersion = ReadInteger(
            path,
            SettingsSection,
            nameof(SchemaVersion),
            CurrentSchemaVersion);

        var enabled = string.Equals(
            ReadString(
                path,
                SettingsSection,
                nameof(Enabled),
                Defaults.Enabled ? "true" : "false"),
            "true",
            StringComparison.Ordinal);

        if (schemaVersion == LegacySchemaVersion)
        {
            return new Settings(
                enabled,
                new[]
                {
                    ReadRepeatSet(
                        path,
                        SettingsSection),
                });
        }

        if (schemaVersion != CurrentSchemaVersion)
        {
            return Defaults;
        }

        var count = ReadInteger(
            path,
            SettingsSection,
            "RepeatSetCount",
            MinRepeatSetCount);
        if (count < MinRepeatSetCount ||
            count > MaxRepeatSetCount)
        {
            return Defaults;
        }

        var repeatSets =
            new RepeatSettingSet[count];
        for (var index = 0; index < count; ++index)
        {
            repeatSets[index] = ReadRepeatSet(
                path,
                RepeatSetSection(index));
        }

        return new Settings(
            enabled,
            repeatSets);
    }

    public static Settings Save(
        string path,
        Settings settings) =>
        Save(
            path,
            settings,
            enabledEdited: true);

    public static Settings Save(
        string path,
        Settings settings,
        bool enabledEdited)
    {
        ArgumentNullException.ThrowIfNull(settings);

        path = System.IO.Path.GetFullPath(path);
        if (!TryValidate(
                settings,
                out var error))
        {
            throw new ArgumentOutOfRangeException(
                nameof(settings),
                error);
        }

        EnsureParentDirectory(path);
        var durableEnabled =
            PublishV2(
                path,
                settings,
                enabledEdited);

        return new Settings(
            durableEnabled,
            settings.RepeatSets);
    }

    public static Settings Patch(
        string path,
        SettingsPatch patch)
    {
        ArgumentNullException.ThrowIfNull(patch);

        path = System.IO.Path.GetFullPath(path);
        var current = Load(path);
        if (patch.IsEmpty)
        {
            return current;
        }

        return Save(
            path,
            patch.Apply(current),
            enabledEdited:
                patch.Enabled is not null);
    }

    private static RepeatSettingSet ReadRepeatSet(
        string path,
        string section)
    {
        var defaults = RepeatSettingSet.Default;

        var initialDelayMs = ReadInteger(
            path,
            section,
            nameof(InitialDelayMs),
            defaults.InitialDelayMs);
        if (initialDelayMs < InitialDelayMinMs ||
            initialDelayMs > InitialDelayMaxMs)
        {
            initialDelayMs = defaults.InitialDelayMs;
        }

        var repeatIntervalMs = ReadInteger(
            path,
            section,
            nameof(RepeatIntervalMs),
            defaults.RepeatIntervalMs);
        if (repeatIntervalMs < RepeatIntervalMinMs ||
            repeatIntervalMs > RepeatIntervalMaxMs)
        {
            repeatIntervalMs =
                defaults.RepeatIntervalMs;
        }

        var presetText = ReadString(
            path,
            section,
            nameof(TargetPreset),
            defaults.TargetPreset.ToString());
        var targetPreset = presetText switch
        {
            nameof(RepeatBoost.Settings.TargetPreset.AllKeys) =>
                RepeatBoost.Settings.TargetPreset.AllKeys,
            nameof(RepeatBoost.Settings.TargetPreset.Custom) =>
                RepeatBoost.Settings.TargetPreset.Custom,
            _ =>
                RepeatBoost.Settings.TargetPreset.ArrowKeys,
        };

        var customTargetText = ReadString(
            path,
            section,
            "CustomTargetKeys",
            string.Empty);
        _ = CustomTargetSet.TryParsePersistedString(
            customTargetText,
            out var customTargets);

        return new RepeatSettingSet(
            initialDelayMs,
            repeatIntervalMs,
            targetPreset,
            customTargets);
    }

    private static int ReadInteger(
        string path,
        string section,
        string key,
        int defaultValue)
    {
        var raw = ReadString(
            path,
            section,
            key,
            defaultValue.ToString(
                System.Globalization.CultureInfo.InvariantCulture));

        return int.TryParse(
            raw,
            System.Globalization.CultureInfo.InvariantCulture,
            out var value)
            ? value
            : defaultValue;
    }

    private static string ReadString(
        string path,
        string section,
        string key,
        string defaultValue)
    {
        var buffer =
            new StringBuilder(StringBufferSize);

        _ = GetPrivateProfileString(
            section,
            key,
            defaultValue,
            buffer,
            buffer.Capacity,
            path);

        return buffer.ToString();
    }

    private static bool TryReadDurableEnabled(
        string path,
        out bool enabled)
    {
        var raw = ReadString(
            path,
            SettingsSection,
            nameof(Enabled),
            string.Empty);

        if (string.Equals(
                raw,
                "true",
                StringComparison.Ordinal))
        {
            enabled = true;
            return true;
        }

        if (string.Equals(
                raw,
                "false",
                StringComparison.Ordinal))
        {
            enabled = false;
            return true;
        }

        enabled = false;
        return false;
    }

    private static string RepeatSetSection(
        int index) =>
        $"RepeatSet{index}";

    private static void EnsureParentDirectory(
        string path)
    {
        var directory =
            System.IO.Path.GetDirectoryName(path)
            ?? throw new InvalidOperationException(
                "Settings path has no directory.");

        Directory.CreateDirectory(directory);
    }

    private static bool PublishV2(
        string path,
        Settings settings,
        bool enabledEdited)
    {
        // Publish all indexed set content before making the v2 collection
        // authoritative. An existing v1 document remains readable until
        // SchemaVersion=2 is written last.
        for (var index = 0;
             index < settings.RepeatSets.Count;
             ++index)
        {
            var setting =
                settings.RepeatSets[index];
            var section =
                RepeatSetSection(index);

            Write(
                path,
                section,
                nameof(InitialDelayMs),
                setting.InitialDelayMs.ToString(
                    System.Globalization.CultureInfo.InvariantCulture));
            Write(
                path,
                section,
                nameof(RepeatIntervalMs),
                setting.RepeatIntervalMs.ToString(
                    System.Globalization.CultureInfo.InvariantCulture));
            Write(
                path,
                section,
                nameof(TargetPreset),
                setting.TargetPreset.ToString());
            Write(
                path,
                section,
                "CustomTargetKeys",
                setting.CustomTargets.ToPersistedString());
        }

        var durableEnabled = settings.Enabled;
        if (enabledEdited ||
            !TryReadDurableEnabled(
                path,
                out durableEnabled))
        {
            durableEnabled = settings.Enabled;
            Write(
                path,
                SettingsSection,
                nameof(Enabled),
                durableEnabled ? "true" : "false");
        }

        Write(
            path,
            SettingsSection,
            "RepeatSetCount",
            settings.RepeatSets.Count.ToString(
                System.Globalization.CultureInfo.InvariantCulture));
        Write(
            path,
            SettingsSection,
            nameof(SchemaVersion),
            CurrentSchemaVersion.ToString(
                System.Globalization.CultureInfo.InvariantCulture));

        Flush(path);
        return durableEnabled;
    }

    private static void Write(
        string path,
        string section,
        string key,
        string value)
    {
        if (WritePrivateProfileString(
                section,
                key,
                value,
                path))
        {
            return;
        }

        var error = Marshal.GetLastWin32Error();
        var platformError =
            error == 0
                ? null
                : new Win32Exception(error);
        throw new IOException(
            platformError?.Message,
            platformError);
    }

    private static void Flush(string path)
    {
        // Match the Engine Profile API contract: cache flush is best-effort.
        // WritePrivateProfileStringW(nullptr, nullptr, nullptr, path) is not
        // a durable mutation result and its FALSE return must not turn a
        // successful sequence of key writes into a save failure.
        _ = WritePrivateProfileString(
            null,
            null,
            null,
            path);
    }

    [DllImport(
        "kernel32.dll",
        CharSet = CharSet.Unicode,
        EntryPoint = "GetPrivateProfileStringW")]
    private static extern uint GetPrivateProfileString(
        string appName,
        string keyName,
        string defaultValue,
        StringBuilder returnedString,
        int size,
        string fileName);

    [DllImport(
        "kernel32.dll",
        CharSet = CharSet.Unicode,
        SetLastError = true,
        EntryPoint = "WritePrivateProfileStringW")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool WritePrivateProfileString(
        string? appName,
        string? keyName,
        string? value,
        string fileName);
}

public sealed record SettingsPatch(
    bool? Enabled = null,
    int? InitialDelayMs = null,
    int? RepeatIntervalMs = null,
    TargetPreset? TargetPreset = null,
    CustomTargetSet? CustomTargets = null,
    IReadOnlyList<RepeatSettingSet>? RepeatSets = null)
{
    public bool IsEmpty =>
        Enabled is null &&
        InitialDelayMs is null &&
        RepeatIntervalMs is null &&
        TargetPreset is null &&
        CustomTargets is null &&
        RepeatSets is null;

    public Settings Apply(Settings current)
    {
        var sets =
            (RepeatSets ?? current.RepeatSets)
                .ToArray();

        if (sets.Length == 0)
        {
            sets =
                new[] { RepeatSettingSet.Default };
        }

        if (InitialDelayMs is not null ||
            RepeatIntervalMs is not null ||
            TargetPreset is not null ||
            CustomTargets is not null)
        {
            var first = sets[0];
            sets[0] = first with
            {
                InitialDelayMs =
                    InitialDelayMs ??
                    first.InitialDelayMs,
                RepeatIntervalMs =
                    RepeatIntervalMs ??
                    first.RepeatIntervalMs,
                TargetPreset =
                    TargetPreset ??
                    first.TargetPreset,
                CustomTargets =
                    CustomTargets ??
                    first.CustomTargets,
            };
        }

        return new Settings(
            Enabled ?? current.Enabled,
            sets);
    }
}
