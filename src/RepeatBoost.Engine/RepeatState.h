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
    RepeatState(TargetPreset preset, TimingSettings timing) noexcept;

    [[nodiscard]] TransitionEffects OnInput(const InputEvent& input, bool bEffectiveEnabled);
    [[nodiscard]] TransitionEffects OnTimerDue(UINT64 ullGeneration, bool bEffectiveEnabled);
    [[nodiscard]] TransitionEffects OnSyntheticSendSucceeded(UINT64 ullGeneration);
    [[nodiscard]] TransitionEffects Stop(StopReason reason);
    void ApplySettings(TargetPreset preset, TimingSettings timing) noexcept;

    [[nodiscard]] bool TryGetCurrentSession(RepeatSessionView& view) const noexcept;
    [[nodiscard]] TargetPreset Preset() const noexcept;
    [[nodiscard]] TimingSettings Timing() const noexcept;

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
        RepeatPhase phase{RepeatPhase::WaitingForTimer};
    };

    [[nodiscard]] UINT64 NextGeneration() noexcept;

    void InvalidateCurrent(TransitionEffects& effects) noexcept;
    void RevokeAllClaims() noexcept;
    void ApplyStopClaimPolicy(StopReason reason) noexcept;
    void StartSession(
        DWORD dwVirtualKey,
        const KeyIdentity& key,
        UINT32 uFirstWaitMs,
        TransitionEffects& effects) noexcept;

    TargetPreset preset_;
    TimingSettings timing_;
    HeldKey keys_[512]{};
    bool bHasCurrent_{};
    RepeatSession current_{};
    UINT64 ullGenerationCounter_{};
};
} // namespace repeatboost::engine
