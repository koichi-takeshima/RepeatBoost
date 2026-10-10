#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace repeatboost::engine
{
struct KeyIdentity
{
    UINT8 byScanCode{};
    bool bExtended{};
};

enum class TargetPreset
{
    ArrowKeys,
    AllKeys,
    Custom,
};

struct CustomTargetSet
{
    UINT64 ullMasks[4]{};

    [[nodiscard]] bool Contains(DWORD dwVirtualKey) const noexcept;
    void Set(DWORD dwVirtualKey, bool bIncluded = true) noexcept;

    bool operator==(const CustomTargetSet&) const = default;
};

struct TargetSettings
{
    TargetPreset preset{TargetPreset::ArrowKeys};
    CustomTargetSet customTargets{};

    bool operator==(const TargetSettings&) const = default;
};

enum class InputOrigin
{
    Physical,
    Injected,
};

enum class KeyAction
{
    Down,
    Up,
};

enum class HookDecision
{
    Pass,
    Suppress,
};

enum class StopReason
{
    DifferentPhysicalFirstDown,
    ForegroundChanged,
    SessionUnavailable,
    Suspend,
    EndSession,
    Disable,
    SettingsReload,
    RuntimeSafetyFailure,
    TimerFailure,
    InjectionFailure,
    Exit,
};

struct TimingSettings
{
    UINT32 uInitialDelayMs{250};
    UINT32 uRepeatIntervalMs{33};

    bool operator==(const TimingSettings&) const = default;
};

inline constexpr UINT kMaxRepeatSettingSets = 16;

struct RepeatSettingSet
{
    TargetSettings target{};
    TimingSettings timing{};

    bool operator==(const RepeatSettingSet&) const = default;
};

struct RepeatSettings
{
    RepeatSettingSet sets[kMaxRepeatSettingSets]{};
    UINT uCount{1};

    bool operator==(const RepeatSettings&) const = default;
};

struct InputEvent
{
    DWORD dwVirtualKey{};
    KeyIdentity key{};
    InputOrigin origin{InputOrigin::Physical};
    KeyAction action{KeyAction::Down};
};

struct TransitionEffects
{
    HookDecision hookDecision{HookDecision::Pass};
    bool bCancelTimer{};
    bool bArmTimer{};
    UINT32 uArmTimerMs{};
    bool bRequestSyntheticKeyDown{};
    UINT64 ullGeneration{};
};

struct RepeatSessionView
{
    DWORD dwVirtualKey{};
    KeyIdentity key{};
    UINT64 ullGeneration{};
};

class RepeatState final
{
public:
    explicit RepeatState(RepeatSettings settings) noexcept;
    RepeatState(TargetSettings target, TimingSettings timing) noexcept;
    RepeatState(TargetPreset preset, TimingSettings timing) noexcept;

    [[nodiscard]] TransitionEffects OnInput(const InputEvent& input, bool bEffectiveEnabled);
    [[nodiscard]] TransitionEffects OnTimerDue(UINT64 ullGeneration, bool bEffectiveEnabled);
    [[nodiscard]] TransitionEffects OnSyntheticSendSucceeded(UINT64 ullGeneration);
    [[nodiscard]] TransitionEffects Stop(StopReason reason);
    void ApplySettings(RepeatSettings settings) noexcept;
    void ApplySettings(TargetSettings target, TimingSettings timing) noexcept;

    [[nodiscard]] bool TryGetCurrentSession(RepeatSessionView& view) const noexcept;
    [[nodiscard]] TargetPreset Preset() const noexcept;
    [[nodiscard]] TimingSettings Timing() const noexcept;
    [[nodiscard]] RepeatSettings Settings() const noexcept;

private:
    enum class RepeatPhase
    {
        WaitingForTimer,
        AwaitingSyntheticResult,
    };

    struct HeldKey
    {
        bool bHeld{};
        bool bClaimed{};
    };

    struct RepeatSession
    {
        DWORD dwVirtualKey{};
        KeyIdentity key{};
        UINT64 ullGeneration{};
        TimingSettings timing{};
        RepeatPhase phase{RepeatPhase::WaitingForTimer};
    };

    [[nodiscard]] UINT64 NextGeneration() noexcept;

    void InvalidateCurrent(TransitionEffects& effects) noexcept;
    void RevokeAllClaims() noexcept;
    void ApplyStopClaimPolicy(StopReason reason) noexcept;
    [[nodiscard]] const RepeatSettingSet* FindMatchingSet(
        DWORD dwVirtualKey) const noexcept;
    void StartSession(
        DWORD dwVirtualKey,
        const KeyIdentity& key,
        TimingSettings timing,
        TransitionEffects& effects) noexcept;

    RepeatSettings settings_;
    HeldKey keys_[512]{};
    bool bHasCurrent_{};
    RepeatSession current_{};
    UINT64 ullGenerationCounter_{};
};
} // namespace repeatboost::engine
