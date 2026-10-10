using Microsoft.UI.Windowing;
using System.ComponentModel;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;
using Windows.Graphics;

namespace RepeatBoost.Settings;

internal readonly record struct SettingsWindowPlacement(
    RectInt32 NormalBounds,
    bool Maximized);

internal static class SettingsWindowPlacementStore
{
    private const string Section = "SettingsWindow";
    private const int StringBufferSize = 64;

    public static bool TryLoad(
        string path,
        out SettingsWindowPlacement placement)
    {
        path = Path.GetFullPath(path);
        placement = default;

        if (!TryReadInt(path, "X", out var x) ||
            !TryReadInt(path, "Y", out var y) ||
            !TryReadInt(path, "Width", out var width) ||
            !TryReadInt(path, "Height", out var height) ||
            width <= 0 ||
            height <= 0)
        {
            return false;
        }

        var maximized = TryReadBool(
            path,
            "Maximized",
            out var parsedMaximized)
            ? parsedMaximized
            : false;

        placement = new SettingsWindowPlacement(
            new RectInt32(
                x,
                y,
                width,
                height),
            maximized);
        return true;
    }

    public static void Save(
        string path,
        SettingsWindowPlacement placement)
    {
        path = Path.GetFullPath(path);
        var bounds = placement.NormalBounds;
        if (bounds.Width <= 0 ||
            bounds.Height <= 0)
        {
            throw new ArgumentOutOfRangeException(
                nameof(placement),
                "Window placement size must be positive.");
        }

        EnsureParentDirectory(path);

        Write(
            path,
            "X",
            bounds.X.ToString(
                CultureInfo.InvariantCulture));
        Write(
            path,
            "Y",
            bounds.Y.ToString(
                CultureInfo.InvariantCulture));
        Write(
            path,
            "Width",
            bounds.Width.ToString(
                CultureInfo.InvariantCulture));
        Write(
            path,
            "Height",
            bounds.Height.ToString(
                CultureInfo.InvariantCulture));
        Write(
            path,
            "Maximized",
            placement.Maximized
                ? "true"
                : "false");
        Flush(path);
    }

    public static RectInt32 ConstrainToWorkArea(
        RectInt32 bounds,
        RectInt32 workArea)
    {
        if (bounds.Width <= 0 ||
            bounds.Height <= 0)
        {
            throw new ArgumentOutOfRangeException(
                nameof(bounds));
        }

        if (workArea.Width <= 0 ||
            workArea.Height <= 0)
        {
            throw new ArgumentOutOfRangeException(
                nameof(workArea));
        }

        var width = Math.Min(
            bounds.Width,
            workArea.Width);
        var height = Math.Min(
            bounds.Height,
            workArea.Height);

        var left = (long)workArea.X;
        var top = (long)workArea.Y;
        var rightMostX =
            left +
            workArea.Width -
            width;
        var bottomMostY =
            top +
            workArea.Height -
            height;

        var x = Math.Clamp(
            (long)bounds.X,
            left,
            rightMostX);
        var y = Math.Clamp(
            (long)bounds.Y,
            top,
            bottomMostY);

        return new RectInt32(
            checked((int)x),
            checked((int)y),
            width,
            height);
    }

    public static void UpdateTracking(
        OverlappedPresenterState state,
        RectInt32 currentBounds,
        ref RectInt32? normalBounds,
        ref bool maximized)
    {
        switch (state)
        {
            case OverlappedPresenterState.Restored:
                if (currentBounds.Width > 0 &&
                    currentBounds.Height > 0)
                {
                    normalBounds =
                        currentBounds;
                }

                maximized = false;
                break;

            case OverlappedPresenterState.Maximized:
                maximized = true;
                break;

            case OverlappedPresenterState.Minimized:
                break;
        }
    }

    private static bool TryReadInt(
        string path,
        string key,
        out int value)
    {
        var raw = ReadString(
            path,
            key,
            string.Empty);

        return int.TryParse(
            raw,
            NumberStyles.Integer,
            CultureInfo.InvariantCulture,
            out value);
    }

    private static bool TryReadBool(
        string path,
        string key,
        out bool value)
    {
        var raw = ReadString(
            path,
            key,
            string.Empty);

        if (string.Equals(
                raw,
                "true",
                StringComparison.Ordinal))
        {
            value = true;
            return true;
        }

        if (string.Equals(
                raw,
                "false",
                StringComparison.Ordinal))
        {
            value = false;
            return true;
        }

        value = false;
        return false;
    }

    private static string ReadString(
        string path,
        string key,
        string defaultValue)
    {
        var buffer =
            new StringBuilder(
                StringBufferSize);

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
            Path.GetDirectoryName(path)
            ?? throw new InvalidOperationException(
                "Settings path has no directory.");

        Directory.CreateDirectory(
            directory);
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

        var error =
            Marshal.GetLastWin32Error();
        var platformError =
            error == 0
                ? null
                : new Win32Exception(
                    error);

        throw new IOException(
            platformError?.Message,
            platformError);
    }

    private static void Flush(
        string path)
    {
        _ = WritePrivateProfileString(
            null,
            null,
            null,
            path);
    }

    [DllImport(
        "kernel32.dll",
        CharSet = CharSet.Unicode,
        EntryPoint =
            "GetPrivateProfileStringW")]
    private static extern uint
        GetPrivateProfileString(
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
        EntryPoint =
            "WritePrivateProfileStringW")]
    [return: MarshalAs(
        UnmanagedType.Bool)]
    private static extern bool
        WritePrivateProfileString(
            string? appName,
            string? keyName,
            string? value,
            string fileName);
}
