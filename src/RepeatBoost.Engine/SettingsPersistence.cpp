#include "SettingsPersistence.h"

#include <windows.h>
#include <pathcch.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>

#pragma comment(lib, "Pathcch.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Shlwapi.lib")

namespace repeatboost::settings
{
namespace
{
constexpr wchar_t kSettingsSection[] = L"Settings";
constexpr DWORD kProfileValueCapacity = 128;
constexpr int kSchemaVersion = 1;
constexpr int kInitialDelayMinMs = 0;
constexpr int kInitialDelayMaxMs = 250;
constexpr int kRepeatIntervalMinMs = 5;
constexpr int kRepeatIntervalMaxMs = 33;
constexpr SettingsDocument kSafeDefaults{};

bool ReadProfileValue(
    const wchar_t* pszPath,
    const wchar_t* pszKey,
    wchar_t* pszValue,
    const DWORD dwValueCapacity)
{
    if (pszPath == nullptr ||
        pszKey == nullptr ||
        pszValue == nullptr ||
        dwValueCapacity == 0)
    {
        return false;
    }

    pszValue[0] = L'\0';

    const DWORD dwCopied = GetPrivateProfileStringW(
        kSettingsSection, pszKey, L"", pszValue, dwValueCapacity, pszPath);

    return dwCopied != 0 && dwCopied != dwValueCapacity - 1;
}

bool ReadProfileInt(
    const wchar_t* pszPath,
    const wchar_t* pszKey,
    int& nValue)
{
    wchar_t szRawValue[kProfileValueCapacity]{};
    if (!ReadProfileValue(pszPath, pszKey, szRawValue, ARRAYSIZE(szRawValue))) return false;

    LONGLONG llConverted = 0;
    if (StrToInt64ExW(
            szRawValue,
            STIF_DEFAULT,
            &llConverted) == FALSE ||
        llConverted < static_cast<LONGLONG>(MININT) ||
        llConverted > static_cast<LONGLONG>(MAXINT))
    {
        return false;
    }

    nValue = static_cast<int>(llConverted);
    return true;
}

bool WriteProfileValue(
    const wchar_t* pszPath,
    const wchar_t* pszKey,
    const wchar_t* pszValue)
{
    return WritePrivateProfileStringW(kSettingsSection, pszKey, pszValue, pszPath) != FALSE;
}

void FlushProfileCache(
    const wchar_t* pszPath) noexcept
{
    (void)WritePrivateProfileStringW(nullptr, nullptr, nullptr, pszPath);
}

bool PrepareSettingsDirectory(
    const wchar_t* pszPath)
{
    wchar_t szDirectory[MAX_PATH]{};
    if (pszPath == nullptr ||
        FAILED(StringCchCopyW(
            szDirectory,
            ARRAYSIZE(szDirectory),
            pszPath)))
    {
        return false;
    }

    if (FAILED(PathCchRemoveFileSpec(
            szDirectory,
            ARRAYSIZE(szDirectory))) ||
        szDirectory[0] == L'\0')
    {
        return false;
    }

    const DWORD dwAttributes = GetFileAttributesW(szDirectory);
    if (dwAttributes != INVALID_FILE_ATTRIBUTES) return (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

    const DWORD dwError = GetLastError();
    if (dwError != ERROR_FILE_NOT_FOUND && dwError != ERROR_PATH_NOT_FOUND) return false;

    if (CreateDirectoryW(szDirectory, nullptr) != FALSE) return true;
    if (GetLastError() != ERROR_ALREADY_EXISTS) return false;

    const DWORD dwExistingAttributes = GetFileAttributesW(szDirectory);
    return dwExistingAttributes != INVALID_FILE_ATTRIBUTES &&
           (dwExistingAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool Validate(
    const SettingsDocument& settings) noexcept
{
    return
        settings.nSchemaVersion == kSchemaVersion &&
        settings.nInitialDelayMs >= kInitialDelayMinMs &&
        settings.nInitialDelayMs <= kInitialDelayMaxMs &&
        settings.nRepeatIntervalMs >= kRepeatIntervalMinMs &&
        settings.nRepeatIntervalMs <= kRepeatIntervalMaxMs &&
        (settings.targetPreset == TargetPreset::ArrowKeys ||
         settings.targetPreset == TargetPreset::AllKeys);
}
} // namespace

SettingsStore::SettingsStore(
    const wchar_t* pszPath) noexcept
{
    if (pszPath == nullptr ||
        FAILED(StringCchCopyW(
            szPath_,
            ARRAYSIZE(szPath_),
            pszPath)))
    {
        szPath_[0] = L'\0';
    }
}


SettingsDocument SettingsStore::Load() const
{
    if (szPath_[0] == L'\0') return kSafeDefaults;

    SettingsDocument settings = kSafeDefaults;
    wchar_t szValue[kProfileValueCapacity]{};

    if (!ReadProfileInt(szPath_, L"SchemaVersion", settings.nSchemaVersion)) return kSafeDefaults;

    if (!ReadProfileValue(szPath_, L"Enabled", szValue, ARRAYSIZE(szValue))) return kSafeDefaults;

    if (wcscmp(szValue, L"true") == 0)
    {
        settings.bEnabled = true;
    }
    else if (wcscmp(szValue, L"false") == 0)
    {
        settings.bEnabled = false;
    }
    else
    {
        return kSafeDefaults;
    }

    if (!ReadProfileInt(szPath_, L"InitialDelayMs", settings.nInitialDelayMs) ||
        !ReadProfileInt(szPath_, L"RepeatIntervalMs", settings.nRepeatIntervalMs))
    {
        return kSafeDefaults;
    }

    if (!ReadProfileValue(szPath_, L"TargetPreset", szValue, ARRAYSIZE(szValue))) return kSafeDefaults;

    if (wcscmp(szValue, L"ArrowKeys") == 0)
    {
        settings.targetPreset =
            TargetPreset::ArrowKeys;
    }
    else if (wcscmp(szValue, L"AllKeys") == 0)
    {
        settings.targetPreset =
            TargetPreset::AllKeys;
    }
    else
    {
        return kSafeDefaults;
    }

    return Validate(settings)
        ? settings
        : kSafeDefaults;
}

bool
SettingsStore::PatchEnabled(const bool bEnabled) const
{
    if (!PrepareSettingsDirectory(szPath_) ||
        !WriteProfileValue(szPath_, L"Enabled", bEnabled ? L"true" : L"false"))
    {
        return false;
    }

    FlushProfileCache(szPath_);
    return true;
}

bool
SettingsStore::DefaultSettingsPath(
    wchar_t* pszPath,
    const SIZE_T cchPathCapacity) noexcept
{
    if (pszPath == nullptr || cchPathCapacity == 0) return false;

    pszPath[0] = L'\0';

    PWSTR pszKnownFolder = nullptr;
    const HRESULT hrKnownFolder =
        SHGetKnownFolderPath(
            FOLDERID_LocalAppData,
            KF_FLAG_DEFAULT,
            nullptr,
            &pszKnownFolder);

    if (FAILED(hrKnownFolder) ||
        pszKnownFolder == nullptr)
    {
        if (pszKnownFolder != nullptr)
        {
            CoTaskMemFree(pszKnownFolder);
        }
        return false;
    }

    wchar_t szDirectory[MAX_PATH]{};
    const HRESULT hrDirectory =
        PathCchCombine(
            szDirectory,
            ARRAYSIZE(szDirectory),
            pszKnownFolder,
            L"RepeatBoost");

    CoTaskMemFree(pszKnownFolder);

    if (FAILED(hrDirectory)) return false;

    if (FAILED(PathCchCombine(
            pszPath,
            cchPathCapacity,
            szDirectory,
            L"settings.ini")))
    {
        pszPath[0] = L'\0';
        return false;
    }

    return true;
}


} // namespace repeatboost::settings
