namespace RepeatBoost.Settings;

public static class SettingsPaths
{
    public static string GetDefaultSettingsFilePath()
    {
        var localAppData = Environment.GetFolderPath(
            Environment.SpecialFolder.LocalApplicationData);

        return Path.Combine(
            localAppData,
            "RepeatBoost",
            "settings.ini");
    }

    public static string GetEngineExecutablePath() =>
        Path.GetFullPath(
            Path.Combine(
                AppContext.BaseDirectory,
                "..",
                "RepeatBoost.Engine.exe"));
}
