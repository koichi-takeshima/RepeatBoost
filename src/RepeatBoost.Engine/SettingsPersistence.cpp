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
constexpr SIZE_T kCustomTargetHexLength = 64;
constexpr int kLegacySchemaVersion = 1;
constexpr int kCurrentSchemaVersion = 2;
constexpr int kInitialDelayMinMs = 0;
constexpr int kInitialDelayMaxMs = 250;
constexpr int kRepeatIntervalMinMs = 5;
constexpr int kRepeatIntervalMaxMs = 33;
constexpr DWORD kExcludedApplicationReadCapacity =
    static_cast<DWORD>(
        kExcludedApplicationNameCapacity + 1);
constexpr SettingsDocument kSafeDefaults{};

bool ReadProfileValue(
    const wchar_t* pszPath,
    const wchar_t* pszSection,
    const wchar_t* pszKey,
    wchar_t* pszValue,
    const DWORD dwValueCapacity)
{
    if (pszPath == nullptr ||
        pszSection == nullptr ||
        pszKey == nullptr ||
        pszValue == nullptr ||
        dwValueCapacity == 0)
    {
        return false;
    }

    pszValue[0] = L'\0';

    const DWORD dwCopied = GetPrivateProfileStringW(
        pszSection,
        pszKey,
        L"",
        pszValue,
        dwValueCapacity,
        pszPath);

    return dwCopied != 0 &&
           dwCopied != dwValueCapacity - 1;
}

bool ReadProfileInt(
    const wchar_t* pszPath,
    const wchar_t* pszSection,
    const wchar_t* pszKey,
    int& nValue)
{
    wchar_t szRawValue[kProfileValueCapacity]{};
    if (!ReadProfileValue(
            pszPath,
            pszSection,
            pszKey,
            szRawValue,
            ARRAYSIZE(szRawValue)))
    {
        return false;
    }

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

[[nodiscard]] int HexDigit(const wchar_t ch) noexcept
{
    if (ch >= L'0' && ch <= L'9') return ch - L'0';
    if (ch >= L'a' && ch <= L'f') return ch - L'a' + 10;
    if (ch >= L'A' && ch <= L'F') return ch - L'A' + 10;
    return -1;
}

[[nodiscard]] bool TryParseCustomTargets(
    const wchar_t* pszValue,
    engine::CustomTargetSet& targets) noexcept
{
    targets = {};
    if (pszValue == nullptr) return false;

    for (SIZE_T uIndex = 0;
         uIndex < kCustomTargetHexLength;
         ++uIndex)
    {
        if (pszValue[uIndex] == L'\0') return false;
    }
    if (pszValue[kCustomTargetHexLength] != L'\0') return false;

    for (UINT uMask = 0; uMask < 4; ++uMask)
    {
        UINT64 ullValue = 0;
        for (UINT uDigit = 0; uDigit < 16; ++uDigit)
        {
            const int nHex =
                HexDigit(pszValue[uMask * 16 + uDigit]);
            if (nHex < 0)
            {
                targets = {};
                return false;
            }

            ullValue =
                (ullValue << 4) |
                static_cast<UINT64>(nHex);
        }

        targets.ullMasks[uMask] = ullValue;
    }

    return true;
}

bool ParseTargetPreset(
    const wchar_t* pszValue,
    TargetPreset& preset) noexcept
{
    if (wcscmp(pszValue, L"ArrowKeys") == 0)
    {
        preset = TargetPreset::ArrowKeys;
        return true;
    }

    if (wcscmp(pszValue, L"AllKeys") == 0)
    {
        preset = TargetPreset::AllKeys;
        return true;
    }

    if (wcscmp(pszValue, L"Custom") == 0)
    {
        preset = TargetPreset::Custom;
        return true;
    }

    return false;
}

bool ValidateRepeatSet(
    const engine::RepeatSettingSet& setting) noexcept
{
    return
        setting.timing.uInitialDelayMs <=
            static_cast<UINT32>(kInitialDelayMaxMs) &&
        setting.timing.uRepeatIntervalMs >=
            static_cast<UINT32>(kRepeatIntervalMinMs) &&
        setting.timing.uRepeatIntervalMs <=
            static_cast<UINT32>(kRepeatIntervalMaxMs) &&
        (setting.target.preset == TargetPreset::ArrowKeys ||
         setting.target.preset == TargetPreset::AllKeys ||
         setting.target.preset == TargetPreset::Custom);
}

bool LoadRepeatSet(
    const wchar_t* pszPath,
    const wchar_t* pszSection,
    engine::RepeatSettingSet& setting)
{
    int nInitialDelayMs = 0;
    int nRepeatIntervalMs = 0;
    wchar_t szValue[kProfileValueCapacity]{};

    if (!ReadProfileInt(
            pszPath,
            pszSection,
            L"InitialDelayMs",
            nInitialDelayMs) ||
        !ReadProfileInt(
            pszPath,
            pszSection,
            L"RepeatIntervalMs",
            nRepeatIntervalMs) ||
        nInitialDelayMs < kInitialDelayMinMs ||
        nInitialDelayMs > kInitialDelayMaxMs ||
        nRepeatIntervalMs < kRepeatIntervalMinMs ||
        nRepeatIntervalMs > kRepeatIntervalMaxMs)
    {
        return false;
    }

    if (!ReadProfileValue(
            pszPath,
            pszSection,
            L"TargetPreset",
            szValue,
            ARRAYSIZE(szValue)) ||
        !ParseTargetPreset(
            szValue,
            setting.target.preset))
    {
        return false;
    }

    setting.timing.uInitialDelayMs =
        static_cast<UINT32>(nInitialDelayMs);
    setting.timing.uRepeatIntervalMs =
        static_cast<UINT32>(nRepeatIntervalMs);

    setting.target.customTargets = {};
    if (ReadProfileValue(
            pszPath,
            pszSection,
            L"CustomTargetKeys",
            szValue,
            ARRAYSIZE(szValue)))
    {
        (void)TryParseCustomTargets(
            szValue,
            setting.target.customTargets);
    }

    return ValidateRepeatSet(setting);
}

bool WriteProfileValue(
    const wchar_t* pszPath,
    const wchar_t* pszSection,
    const wchar_t* pszKey,
    const wchar_t* pszValue)
{
    return WritePrivateProfileStringW(
               pszSection,
               pszKey,
               pszValue,
               pszPath) != FALSE;
}

void FlushProfileCache(
    const wchar_t* pszPath) noexcept
{
    (void)WritePrivateProfileStringW(
        nullptr,
        nullptr,
        nullptr,
        pszPath);
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

    const DWORD dwAttributes =
        GetFileAttributesW(szDirectory);
    if (dwAttributes != INVALID_FILE_ATTRIBUTES)
    {
        return
            (dwAttributes &
             FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    const DWORD dwError = GetLastError();
    if (dwError != ERROR_FILE_NOT_FOUND &&
        dwError != ERROR_PATH_NOT_FOUND)
    {
        return false;
    }

    if (CreateDirectoryW(
            szDirectory,
            nullptr) != FALSE)
    {
        return true;
    }

    if (GetLastError() != ERROR_ALREADY_EXISTS)
        return false;

    const DWORD dwExistingAttributes =
        GetFileAttributesW(szDirectory);
    return
        dwExistingAttributes !=
            INVALID_FILE_ATTRIBUTES &&
        (dwExistingAttributes &
         FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool ReadEnabled(
    const wchar_t* pszPath,
    bool& bEnabled)
{
    wchar_t szValue[kProfileValueCapacity]{};
    if (!ReadProfileValue(
            pszPath,
            kSettingsSection,
            L"Enabled",
            szValue,
            ARRAYSIZE(szValue)))
    {
        return false;
    }

    if (wcscmp(szValue, L"true") == 0)
    {
        bEnabled = true;
        return true;
    }

    if (wcscmp(szValue, L"false") == 0)
    {
        bEnabled = false;
        return true;
    }

    return false;
}

[[nodiscard]] bool IsTrimWhitespace(
    const wchar_t ch) noexcept
{
    return ch == L' ' ||
           ch == L'\t' ||
           ch == L'\r' ||
           ch == L'\n' ||
           ch == L'\v' ||
           ch == L'\f';
}

[[nodiscard]] bool IsInvalidFilenameCharacter(
    const wchar_t ch) noexcept
{
    if (ch < 0x20)
        return true;

    switch (ch)
    {
    case L'<':
    case L'>':
    case L':':
    case L'"':
    case L'/':
    case L'\\':
    case L'|':
    case L'?':
    case L'*':
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool TryNormalizeExcludedApplication(
    const wchar_t* pszValue,
    wchar_t* pszNormalized,
    const SIZE_T cchNormalizedCapacity) noexcept
{
    if (pszValue == nullptr ||
        pszNormalized == nullptr ||
        cchNormalizedCapacity == 0)
    {
        return false;
    }

    pszNormalized[0] = L'\0';

    const wchar_t* pszBegin = pszValue;
    while (*pszBegin != L'\0' &&
           IsTrimWhitespace(*pszBegin))
    {
        ++pszBegin;
    }

    const wchar_t* pszEnd =
        pszBegin + lstrlenW(pszBegin);
    while (pszEnd > pszBegin &&
           IsTrimWhitespace(pszEnd[-1]))
    {
        --pszEnd;
    }

    const SIZE_T cchLength =
        static_cast<SIZE_T>(
            pszEnd - pszBegin);
    if (cchLength <= 4 ||
        cchLength >
            kExcludedApplicationNameCapacity - 1 ||
        cchLength >= cchNormalizedCapacity)
    {
        return false;
    }

    for (SIZE_T uIndex = 0;
         uIndex < cchLength;
         ++uIndex)
    {
        if (IsInvalidFilenameCharacter(
                pszBegin[uIndex]))
        {
            return false;
        }
    }

    if (CompareStringOrdinal(
            pszEnd - 4,
            4,
            L".exe",
            4,
            TRUE) != CSTR_EQUAL)
    {
        return false;
    }

    for (SIZE_T uIndex = 0;
         uIndex < cchLength;
         ++uIndex)
    {
        pszNormalized[uIndex] =
            pszBegin[uIndex];
    }
    pszNormalized[cchLength] = L'\0';
    return true;
}

[[nodiscard]] bool TryAddExcludedApplication(
    ExcludedApplicationList& applications,
    const wchar_t* pszValue) noexcept
{
    if (applications.uCount >=
        kMaxExcludedApplications)
    {
        return false;
    }

    wchar_t szNormalized[
        kExcludedApplicationNameCapacity]{};
    if (!TryNormalizeExcludedApplication(
            pszValue,
            szNormalized,
            ARRAYSIZE(szNormalized)) ||
        applications.Contains(szNormalized))
    {
        return false;
    }

    if (FAILED(StringCchCopyW(
            applications.szBasenames[
                applications.uCount],
            ARRAYSIZE(
                applications.szBasenames[
                    applications.uCount]),
            szNormalized)))
    {
        return false;
    }

    ++applications.uCount;
    return true;
}

[[nodiscard]] ExcludedApplicationList
LoadExcludedApplications(
    const wchar_t* pszPath)
{
    ExcludedApplicationList applications{};

    wchar_t szSelector[4]{};
    if (!ReadProfileValue(
            pszPath,
            kSettingsSection,
            L"ExcludedAppsActive",
            szSelector,
            ARRAYSIZE(szSelector)))
    {
        return applications;
    }

    const wchar_t* pszSection = nullptr;
    if (wcscmp(szSelector, L"A") == 0)
    {
        pszSection = L"ExcludedAppsA";
    }
    else if (wcscmp(szSelector, L"B") == 0)
    {
        pszSection = L"ExcludedAppsB";
    }
    else
    {
        return applications;
    }

    int nCount = 0;
    if (!ReadProfileInt(
            pszPath,
            pszSection,
            L"Count",
            nCount) ||
        nCount < 0 ||
        nCount >
            static_cast<int>(
                kMaxExcludedApplications))
    {
        return applications;
    }

    for (int nIndex = 0;
         nIndex < nCount;
         ++nIndex)
    {
        wchar_t szKey[6] = L"App0";
        if (nIndex < 10)
        {
            szKey[3] =
                static_cast<wchar_t>(
                    L'0' + nIndex);
            szKey[4] = L'\0';
        }
        else
        {
            szKey[3] =
                static_cast<wchar_t>(
                    L'0' + (nIndex / 10));
            szKey[4] =
                static_cast<wchar_t>(
                    L'0' + (nIndex % 10));
            szKey[5] = L'\0';
        }

        wchar_t szValue[
            kExcludedApplicationReadCapacity]{};
        if (!ReadProfileValue(
                pszPath,
                pszSection,
                szKey,
                szValue,
                ARRAYSIZE(szValue)))
        {
            continue;
        }

        (void)TryAddExcludedApplication(
            applications,
            szValue);
    }

    return applications;
}
} // namespace

bool ExcludedApplicationList::Contains(
    const wchar_t* pszBasename) const noexcept
{
    if (pszBasename == nullptr ||
        pszBasename[0] == L'\0')
    {
        return false;
    }

    for (UINT uIndex = 0;
         uIndex < uCount;
         ++uIndex)
    {
        if (CompareStringOrdinal(
                szBasenames[uIndex],
                -1,
                pszBasename,
                -1,
                TRUE) == CSTR_EQUAL)
        {
            return true;
        }
    }

    return false;
}

bool ExcludedApplicationList::operator==(
    const ExcludedApplicationList& other) const noexcept
{
    if (uCount != other.uCount)
        return false;

    for (UINT uIndex = 0;
         uIndex < uCount;
         ++uIndex)
    {
        if (CompareStringOrdinal(
                szBasenames[uIndex],
                -1,
                other.szBasenames[uIndex],
                -1,
                TRUE) != CSTR_EQUAL)
        {
            return false;
        }
    }

    return true;
}

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
    if (szPath_[0] == L'\0')
        return kSafeDefaults;

    int nSchemaVersion = 0;
    if (!ReadProfileInt(
            szPath_,
            kSettingsSection,
            L"SchemaVersion",
            nSchemaVersion))
    {
        return kSafeDefaults;
    }

    SettingsDocument settings =
        kSafeDefaults;
    settings.nSchemaVersion =
        nSchemaVersion;

    if (!ReadEnabled(
            szPath_,
            settings.bEnabled))
    {
        return kSafeDefaults;
    }

    settings.excludedApplications =
        LoadExcludedApplications(szPath_);

    if (nSchemaVersion ==
        kLegacySchemaVersion)
    {
        settings.repeatSettings =
            engine::RepeatSettings{};
        settings.repeatSettings.uCount = 1;

        if (!LoadRepeatSet(
                szPath_,
                kSettingsSection,
                settings.repeatSettings.sets[0]))
        {
            return kSafeDefaults;
        }

        return settings;
    }

    if (nSchemaVersion !=
        kCurrentSchemaVersion)
    {
        return kSafeDefaults;
    }

    int nRepeatSetCount = 0;
    if (!ReadProfileInt(
            szPath_,
            kSettingsSection,
            L"RepeatSetCount",
            nRepeatSetCount) ||
        nRepeatSetCount < 1 ||
        nRepeatSetCount >
            static_cast<int>(
                engine::kMaxRepeatSettingSets))
    {
        return kSafeDefaults;
    }

    settings.repeatSettings =
        engine::RepeatSettings{};
    settings.repeatSettings.uCount =
        static_cast<UINT>(nRepeatSetCount);

    for (int nIndex = 0;
         nIndex < nRepeatSetCount;
         ++nIndex)
    {
        wchar_t szSection[12] = L"RepeatSet0";
        if (nIndex < 10)
        {
            szSection[9] =
                static_cast<wchar_t>(
                    L'0' + nIndex);
        }
        else
        {
            szSection[9] = L'1';
            szSection[10] =
                static_cast<wchar_t>(
                    L'0' + (nIndex - 10));
            szSection[11] = L'\0';
        }

        if (!LoadRepeatSet(
                szPath_,
                szSection,
                settings.repeatSettings
                    .sets[nIndex]))
        {
            return kSafeDefaults;
        }
    }

    return settings;
}

bool SettingsStore::PatchEnabled(
    const bool bEnabled) const
{
    if (!PrepareSettingsDirectory(szPath_) ||
        !WriteProfileValue(
            szPath_,
            kSettingsSection,
            L"Enabled",
            bEnabled ? L"true" : L"false"))
    {
        return false;
    }

    FlushProfileCache(szPath_);
    return true;
}

bool SettingsStore::DefaultSettingsPath(
    wchar_t* pszPath,
    const SIZE_T cchPathCapacity) noexcept
{
    if (pszPath == nullptr ||
        cchPathCapacity == 0)
    {
        return false;
    }

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

    if (FAILED(hrDirectory))
        return false;

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
