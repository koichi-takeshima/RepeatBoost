#include "RepeatState.h"

namespace repeatboost::engine
{
namespace
{
[[nodiscard]] bool IsArrowKey(const DWORD dwVirtualKey) noexcept
{
    return dwVirtualKey == VK_LEFT ||
           dwVirtualKey == VK_UP ||
           dwVirtualKey == VK_RIGHT ||
           dwVirtualKey == VK_DOWN;
}

[[nodiscard]] bool IsModifierKey(const DWORD dwVirtualKey) noexcept
{
    switch (dwVirtualKey)
    {
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
        return true;

    default:
        return false;
    }
}

[[nodiscard]] bool IsToggleKey(const DWORD dwVirtualKey) noexcept
{
    return dwVirtualKey == VK_CAPITAL ||
           dwVirtualKey == VK_NUMLOCK ||
           dwVirtualKey == VK_SCROLL;
}

[[nodiscard]] bool IsFirstVersionAllKeysTarget(const DWORD dwVirtualKey) noexcept
{
    return !IsModifierKey(dwVirtualKey) &&
           !IsToggleKey(dwVirtualKey);
}

[[nodiscard]] bool IsRepeatTarget(
    const DWORD dwVirtualKey,
    const TargetPreset preset) noexcept
{
    switch (preset)
    {
    case TargetPreset::ArrowKeys:
        return IsArrowKey(dwVirtualKey);

    case TargetPreset::AllKeys:
        return IsFirstVersionAllKeysTarget(dwVirtualKey);
    }

    return false;
}
} // namespace

RepeatState::RepeatState(const TargetPreset preset, const TimingSettings timing) noexcept
    : preset_(preset),
      timing_(timing)
{
}

TransitionEffects RepeatState::OnInput(
    const InputEvent& input,
    const bool bEffectiveEnabled)
{
    TransitionEffects effects{};

    if (input.origin == InputOrigin::Injected) return effects;

    const SIZE_T uKeyIndex =
        static_cast<SIZE_T>(input.key.byScanCode) |
        (input.key.bExtended ? 0x100U : 0U);
    auto& keyState = keys_[uKeyIndex];

    if (input.action == KeyAction::Up)
    {
        if (!keyState.bHeld) return effects;

        const bool bWasCurrent =
            bHasCurrent_ &&
            current_.key.byScanCode == input.key.byScanCode &&
            current_.key.bExtended == input.key.bExtended;

        keyState = HeldKey{};

        if (bWasCurrent) InvalidateCurrent(effects);

        return effects;
    }

    if (keyState.bHeld)
    {
        effects.hookDecision =
            keyState.bClaimed ? HookDecision::Suppress : HookDecision::Pass;
        return effects;
    }

    keyState.bHeld = true;

    if (bHasCurrent_) InvalidateCurrent(effects);

    if (!bEffectiveEnabled || !IsRepeatTarget(input.dwVirtualKey, preset_))
    {
        keyState.bClaimed = false;
        return effects;
    }

    keyState.bClaimed = true;

    const auto uFirstWaitMs =
        timing_.uInitialDelayMs == 0
            ? timing_.uRepeatIntervalMs
            : timing_.uInitialDelayMs;

    StartSession(
        input.dwVirtualKey,
        input.key,
        uFirstWaitMs,
        effects);
    return effects;
}

TransitionEffects RepeatState::OnTimerDue(
    const UINT64 ullGeneration,
    const bool bEffectiveEnabled)
{
    TransitionEffects effects{};
    effects.ullGeneration = ullGeneration;

    if (!bHasCurrent_ ||
        current_.ullGeneration != ullGeneration ||
        current_.phase != RepeatPhase::WaitingForTimer)
    {
        return effects;
    }

    if (!bEffectiveEnabled)
    {
        effects = Stop(StopReason::RuntimeSafetyFailure);
        effects.ullGeneration = ullGeneration;
        return effects;
    }

    const SIZE_T uKeyIndex =
        static_cast<SIZE_T>(current_.key.byScanCode) |
        (current_.key.bExtended ? 0x100U : 0U);
    const auto& keyState = keys_[uKeyIndex];
    if (!keyState.bHeld || !keyState.bClaimed)
    {
        InvalidateCurrent(effects);
        effects.ullGeneration = ullGeneration;
        return effects;
    }

    current_.phase = RepeatPhase::AwaitingSyntheticResult;
    effects.bRequestSyntheticKeyDown = true;
    return effects;
}

TransitionEffects RepeatState::OnSyntheticSendSucceeded(
    const UINT64 ullGeneration)
{
    TransitionEffects effects{};
    effects.ullGeneration = ullGeneration;

    if (!bHasCurrent_ ||
        current_.ullGeneration != ullGeneration ||
        current_.phase != RepeatPhase::AwaitingSyntheticResult)
    {
        return effects;
    }

    const SIZE_T uKeyIndex =
        static_cast<SIZE_T>(current_.key.byScanCode) |
        (current_.key.bExtended ? 0x100U : 0U);
    const auto& keyState = keys_[uKeyIndex];
    if (!keyState.bHeld || !keyState.bClaimed)
    {
        InvalidateCurrent(effects);
        effects.ullGeneration = ullGeneration;
        return effects;
    }

    current_.phase = RepeatPhase::WaitingForTimer;
    effects.bArmTimer = true;
    effects.uArmTimerMs = timing_.uRepeatIntervalMs;
    return effects;
}

TransitionEffects RepeatState::Stop(const StopReason reason)
{
    TransitionEffects effects{};
    InvalidateCurrent(effects);
    ApplyStopClaimPolicy(reason);
    return effects;
}

void RepeatState::ApplySettings(
    const TargetPreset preset,
    const TimingSettings timing) noexcept
{
    preset_ = preset;
    timing_ = timing;
}

bool RepeatState::TryGetCurrentSession(
    RepeatSessionView& view) const noexcept
{
    if (!bHasCurrent_) return false;

    view = RepeatSessionView{
        .dwVirtualKey = current_.dwVirtualKey,
        .key = current_.key,
        .ullGeneration = current_.ullGeneration,
    };
    return true;
}

TargetPreset RepeatState::Preset() const noexcept
{
    return preset_;
}

TimingSettings RepeatState::Timing() const noexcept
{
    return timing_;
}

UINT64 RepeatState::NextGeneration() noexcept
{
    ++ullGenerationCounter_;
    if (ullGenerationCounter_ == 0) ++ullGenerationCounter_;

    return ullGenerationCounter_;
}

void RepeatState::InvalidateCurrent(TransitionEffects& effects) noexcept
{
    if (!bHasCurrent_) return;

    effects.bCancelTimer = true;
    effects.ullGeneration = current_.ullGeneration;
    bHasCurrent_ = false;
    current_ = RepeatSession{};
}

void RepeatState::RevokeAllClaims() noexcept
{
    for (auto& keyState : keys_)
    {
        keyState.bClaimed = false;
    }
}

void RepeatState::ApplyStopClaimPolicy(const StopReason reason) noexcept
{
    switch (reason)
    {
    case StopReason::DifferentPhysicalFirstDown:
    case StopReason::ForegroundChanged:
        // Retain old held-key claims so Windows autorepeat cannot silently resume.
        return;

    case StopReason::SessionUnavailable:
    case StopReason::Suspend:
    case StopReason::EndSession:
    case StopReason::Disable:
    case StopReason::SettingsReload:
    case StopReason::RuntimeSafetyFailure:
    case StopReason::TimerFailure:
    case StopReason::InjectionFailure:
    case StopReason::Exit:
        RevokeAllClaims();
        return;
    }
}

void RepeatState::StartSession(
    const DWORD dwVirtualKey,
    const KeyIdentity& key,
    const UINT32 uFirstWaitMs,
    TransitionEffects& effects) noexcept
{
    const auto ullGeneration = NextGeneration();

    current_ = RepeatSession{
        .dwVirtualKey = dwVirtualKey,
        .key = key,
        .ullGeneration = ullGeneration,
        .phase = RepeatPhase::WaitingForTimer,
    };
    bHasCurrent_ = true;

    effects.bArmTimer = true;
    effects.uArmTimerMs = uFirstWaitMs;
    effects.ullGeneration = ullGeneration;
}
} // namespace repeatboost::engine
