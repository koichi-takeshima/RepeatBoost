using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

namespace RepeatBoost.Settings;

public enum TargetPreset
{
    ArrowKeys,
    AllKeys,
}

public sealed record Settings(
    int SchemaVersion,
    bool Enabled,
    int InitialDelayMs,
    int RepeatIntervalMs,
    TargetPreset TargetPreset)
{
    private const string Section = "Settings";
    private const int StringBufferSize = 128;

    public const int CurrentSchemaVersion = 1;
    public const int InitialDelayMinMs = 0;
    public const int InitialDelayMaxMs = 250;
    public const int RepeatIntervalMinMs = 5;
    public const int RepeatIntervalMaxMs = 33;

    public static Settings Defaults { get; } = new(
        CurrentSchemaVersion,
        Enabled: false,
        InitialDelayMaxMs,
        RepeatIntervalMaxMs,
        TargetPreset.ArrowKeys);

    public static bool TryValidate(
        Settings settings,
        out string? error)
    {
        if (settings.SchemaVersion != CurrentSchemaVersion)
        {
            error = $"Unsupported SchemaVersion: {settings.SchemaVersion}";
            return false;
        }

        if (settings.InitialDelayMs < InitialDelayMinMs ||
            settings.InitialDelayMs > InitialDelayMaxMs)
        {
            error = $"InitialDelayMs out of range: {settings.InitialDelayMs}";
            return false;
        }

        if (settings.RepeatIntervalMs < RepeatIntervalMinMs ||
            settings.RepeatIntervalMs > RepeatIntervalMaxMs)
        {
            error = $"RepeatIntervalMs out of range: {settings.RepeatIntervalMs}";
            return false;
        }

        if (!Enum.IsDefined(settings.TargetPreset))
        {
            error = $"Unknown TargetPreset: {settings.TargetPreset}";
            return false;
        }

        error = null;
        return true;
    }

    public static Settings Load(string path)
    {
        path = System.IO.Path.GetFullPath(path);
        var defaults = Defaults;

        var schemaVersion = ReadInteger(
            path,
            nameof(SchemaVersion),
            defaults.SchemaVersion);

        if (schemaVersion != CurrentSchemaVersion)
        {
            return defaults;
        }

        var initialDelayMs = ReadInteger(
            path,
            nameof(InitialDelayMs),
            defaults.InitialDelayMs);
        if (initialDelayMs < InitialDelayMinMs ||
            initialDelayMs > InitialDelayMaxMs)
        {
            initialDelayMs = defaults.InitialDelayMs;
        }

        var repeatIntervalMs = ReadInteger(
            path,
            nameof(RepeatIntervalMs),
            defaults.RepeatIntervalMs);
        if (repeatIntervalMs < RepeatIntervalMinMs ||
            repeatIntervalMs > RepeatIntervalMaxMs)
        {
            repeatIntervalMs = defaults.RepeatIntervalMs;
        }

        var enabled = string.Equals(
            ReadString(
                path,
                nameof(Enabled),
                defaults.Enabled ? "true" : "false"),
            "true",
            StringComparison.Ordinal);

        var presetText = ReadString(
            path,
            nameof(TargetPreset),
            defaults.TargetPreset.ToString());
        var targetPreset = string.Equals(
                presetText,
                nameof(RepeatBoost.Settings.TargetPreset.AllKeys),
                StringComparison.Ordinal)
            ? RepeatBoost.Settings.TargetPreset.AllKeys
            : RepeatBoost.Settings.TargetPreset.ArrowKeys;

        return new Settings(
            schemaVersion,
            enabled,
            initialDelayMs,
            repeatIntervalMs,
            targetPreset);
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

        var updated = patch.Apply(current);
        if (!TryValidate(
                updated,
                out var error))
        {
            throw new ArgumentOutOfRangeException(
                nameof(patch),
                error);
        }

        EnsureParentDirectory(path);
        WritePatch(
            path,
            patch);
        return updated;
    }

    private static int ReadInteger(
        string path,
        string key,
        int defaultValue)
    {
        var raw = ReadString(
            path,
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
        string key,
        string defaultValue)
    {
        var buffer =
            new StringBuilder(StringBufferSize);

        _ = GetPrivateProfileString(
            Section,
            key,
            defaultValue,
            buffer,
            buffer.Capacity,
            path);

        return buffer.ToString();
    }

    private static void EnsureParentDirectory(
        string path)
    {
        var directory =
            System.IO.Path.GetDirectoryName(path)
            ?? throw new InvalidOperationException(
                "Settings path has no directory.");

        Directory.CreateDirectory(directory);
    }

    private static void WritePatch(
        string path,
        SettingsPatch patch)
    {
        if (patch.Enabled is false)
        {
            Write(
                path,
                nameof(Enabled),
                "false");
        }

        if (patch.InitialDelayMs is int initialDelayMs)
        {
            Write(
                path,
                nameof(InitialDelayMs),
                initialDelayMs.ToString(
                    System.Globalization.CultureInfo.InvariantCulture));
        }

        if (patch.RepeatIntervalMs is int repeatIntervalMs)
        {
            Write(
                path,
                nameof(RepeatIntervalMs),
                repeatIntervalMs.ToString(
                    System.Globalization.CultureInfo.InvariantCulture));
        }

        if (patch.TargetPreset is RepeatBoost.Settings.TargetPreset targetPreset)
        {
            Write(
                path,
                nameof(TargetPreset),
                targetPreset.ToString());
        }

        if (patch.Enabled is true)
        {
            Write(
                path,
                nameof(Enabled),
                "true");
        }
    }

    private static void Write(
        string path,
        string key,
        string value)
    {
        if (WritePrivateProfileString(
                Section,
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
        string appName,
        string keyName,
        string value,
        string fileName);
}

public sealed record SettingsPatch(
    bool? Enabled = null,
    int? InitialDelayMs = null,
    int? RepeatIntervalMs = null,
    TargetPreset? TargetPreset = null)
{
    public bool IsEmpty =>
        Enabled is null &&
        InitialDelayMs is null &&
        RepeatIntervalMs is null &&
        TargetPreset is null;

    public Settings Apply(Settings current) => current with
    {
        Enabled = Enabled ?? current.Enabled,
        InitialDelayMs = InitialDelayMs ?? current.InitialDelayMs,
        RepeatIntervalMs = RepeatIntervalMs ?? current.RepeatIntervalMs,
        TargetPreset = TargetPreset ?? current.TargetPreset,
    };
}
