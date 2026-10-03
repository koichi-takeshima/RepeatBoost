using System.Security;
using Microsoft.Win32;

namespace RepeatBoost.Settings;

public enum AutostartState
{
    Disabled,
    Enabled,
    Conflict,
    Error,
}

public sealed record AutostartResult(
    AutostartState State,
    string? Message = null);

public interface IRunKeyAccess
{
    string? Read(string valueName);
    void Write(string valueName, string command);
    void Delete(string valueName);
}

public sealed class CurrentUserRunKeyAccess : IRunKeyAccess
{
    public const string RunKeyPath =
        @"Software\Microsoft\Windows\CurrentVersion\Run";

    public string? Read(string valueName)
    {
        using var key = Registry.CurrentUser.OpenSubKey(
            RunKeyPath,
            writable: false);

        return key?.GetValue(
            valueName,
            defaultValue: null,
            RegistryValueOptions.DoNotExpandEnvironmentNames) as string;
    }

    public void Write(string valueName, string command)
    {
        using var key = Registry.CurrentUser.CreateSubKey(
            RunKeyPath,
            writable: true)
            ?? throw new IOException();

        key.SetValue(
            valueName,
            command,
            RegistryValueKind.String);
    }

    public void Delete(string valueName)
    {
        using var key = Registry.CurrentUser.OpenSubKey(
            RunKeyPath,
            writable: true);

        key?.DeleteValue(valueName, throwOnMissingValue: false);
    }
}

public sealed class AutostartRegistration
{
    public const string ValueName = "RepeatBoost";

    private readonly IRunKeyAccess _runKey;
    private readonly string _expectedCommand;

    public AutostartRegistration(
        IRunKeyAccess runKey,
        string engineExecutablePath)
    {
        _runKey = runKey;
        _expectedCommand =
            $"\"{Path.GetFullPath(engineExecutablePath)}\"";
    }

    public AutostartResult Query()
    {
        try
        {
            var value = _runKey.Read(ValueName);
            if (value is null)
            {
                return new(AutostartState.Disabled);
            }

            if (string.Equals(
                    value,
                    _expectedCommand,
                    StringComparison.OrdinalIgnoreCase))
            {
                return new(AutostartState.Enabled);
            }

            return new(AutostartState.Conflict);
        }
        catch (Exception ex) when (
            ex is IOException ||
            ex is UnauthorizedAccessException ||
            ex is SecurityException)
        {
            return new(AutostartState.Error, ex.Message);
        }
    }

    public AutostartResult SetEnabled(bool enabled)
    {
        var current = Query();
        if (current.State == AutostartState.Error)
        {
            return current;
        }

        if (current.State == AutostartState.Conflict)
        {
            return current;
        }

        try
        {
            if (enabled)
            {
                _runKey.Write(ValueName, _expectedCommand);
            }
            else if (current.State == AutostartState.Enabled)
            {
                _runKey.Delete(ValueName);
            }

            return Query();
        }
        catch (Exception ex) when (
            ex is IOException ||
            ex is UnauthorizedAccessException ||
            ex is SecurityException)
        {
            return new(AutostartState.Error, ex.Message);
        }
    }
}
