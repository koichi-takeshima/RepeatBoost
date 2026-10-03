#pragma once

#include "RepeatState.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace repeatboost::settings
{
using TargetPreset = engine::TargetPreset;

struct SettingsDocument
{
    int nSchemaVersion = 1;
    bool bEnabled = false;
    int nInitialDelayMs = 250;
    int nRepeatIntervalMs = 33;
    TargetPreset targetPreset = TargetPreset::ArrowKeys;

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
