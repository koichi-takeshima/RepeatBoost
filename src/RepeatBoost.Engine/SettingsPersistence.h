#pragma once

#include "RepeatState.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace repeatboost::settings
{
using TargetPreset = engine::TargetPreset;

constexpr UINT kMaxExcludedApplications = 32;
constexpr SIZE_T kExcludedApplicationNameCapacity = 256;

struct ExcludedApplicationList
{
    UINT uCount = 0;
    wchar_t szBasenames
        [kMaxExcludedApplications]
        [kExcludedApplicationNameCapacity]{};

    [[nodiscard]] bool Contains(
        const wchar_t* pszBasename) const noexcept;
    [[nodiscard]] bool operator==(
        const ExcludedApplicationList& other) const noexcept;
};

struct SettingsDocument
{
    int nSchemaVersion = 2;
    bool bEnabled = false;
    engine::RepeatSettings repeatSettings{};
    ExcludedApplicationList excludedApplications{};

    bool operator==(const SettingsDocument&) const = default;
};

class SettingsStore
{
public:
    explicit SettingsStore(const wchar_t* pszPath) noexcept;

    [[nodiscard]] SettingsDocument Load() const;
    [[nodiscard]] bool PatchEnabled(bool bEnabled) const;

    [[nodiscard]] static bool DefaultSettingsPath(
        wchar_t* pszPath,
        SIZE_T cchPathCapacity) noexcept;

private:
    wchar_t szPath_[MAX_PATH]{};
};
} // namespace repeatboost::settings
