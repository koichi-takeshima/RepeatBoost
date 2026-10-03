#include "RepeatRuntime.h"

namespace repeatboost::engine
{
namespace
{
INPUT BuildSyntheticKeyDown(
    const DWORD dwVirtualKey,
    const KeyIdentity& key) noexcept
{
    INPUT inputKeyDown{};
    inputKeyDown.type = INPUT_KEYBOARD;
    if (key.byScanCode != 0)
    {
        inputKeyDown.ki.wVk = 0;
        inputKeyDown.ki.wScan = static_cast<WORD>(key.byScanCode);
        inputKeyDown.ki.dwFlags =
            KEYEVENTF_SCANCODE |
            (key.bExtended ? KEYEVENTF_EXTENDEDKEY : 0);
    }
    else
    {
        inputKeyDown.ki.wVk = static_cast<WORD>(dwVirtualKey);
    }

    return inputKeyDown;
}

} // namespace

RepeatRuntime::RepeatRuntime(const TargetPreset preset, const TimingSettings timing) noexcept
    : state_(preset, timing)
{
}

RepeatRuntime::~RepeatRuntime()
{
    Shutdown();
}

bool RepeatRuntime::Initialize()
{
    if (bReady_) return true;

    bShuttingDown_ = false;

    hRepeatTimer_ = CreateWaitableTimerExW(
        nullptr,
        nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS);

    if (hRepeatTimer_ == nullptr)
    {
        bReady_ = false;
        return false;
    }

    bReady_ = true;
    return true;
}

void RepeatRuntime::Shutdown() noexcept
{
    if (bShuttingDown_)
    {
        return;
    }

    bShuttingDown_ = true;
    ApplyEffects(state_.Stop(StopReason::Exit));
    bReady_ = false;

    if (hRepeatTimer_ != nullptr)
    {
        CancelWaitableTimer(hRepeatTimer_);
        CloseHandle(hRepeatTimer_);
        hRepeatTimer_ = nullptr;
        ullArmedGeneration_ = 0;
    }
}

void RepeatRuntime::Stop(const StopReason reason) noexcept
{
    ApplyEffects(state_.Stop(reason));
}

void RepeatRuntime::ApplySettings(
    const TargetPreset preset,
    const TimingSettings timing) noexcept
{
    state_.ApplySettings(preset, timing);
}

HookDecision RepeatRuntime::ProcessInput(
    const InputEvent& input,
    const bool bEffectiveEnabled) noexcept
{
    const auto effects = state_.OnInput(input, bEffectiveEnabled);

    ApplyEffects(effects);
    return effects.hookDecision;
}

HANDLE RepeatRuntime::TimerHandle() const noexcept
{
    return hRepeatTimer_;
}

UINT64 RepeatRuntime::ConsumeTimerSignalGeneration() noexcept
{
    const UINT64 ullGeneration = ullArmedGeneration_;
    ullArmedGeneration_ = 0;
    return ullGeneration;
}

void RepeatRuntime::HandleTimerSignaled(
    const UINT64 ullGeneration,
    const bool bEffectiveEnabled)
{
    if (ullGeneration != 0) HandleTimerDue(ullGeneration, bEffectiveEnabled);
}

void RepeatRuntime::HandleTimerWaitFailure() noexcept
{
    HandleTimingFailure();

    if (hRepeatTimer_ != nullptr)
    {
        CloseHandle(hRepeatTimer_);
        hRepeatTimer_ = nullptr;
        ullArmedGeneration_ = 0;
    }
}

bool RepeatRuntime::Ready() const noexcept
{
    return bReady_;
}

void RepeatRuntime::ApplyEffects(
    const TransitionEffects& effects) noexcept
{
    if (hRepeatTimer_ == nullptr) return;

    if (effects.bCancelTimer)
    {
        CancelWaitableTimer(hRepeatTimer_);
        ullArmedGeneration_ = 0;
    }

    if (effects.bArmTimer)
    {
        LARGE_INTEGER liDue{};
        liDue.QuadPart = -static_cast<LONGLONG>(effects.uArmTimerMs) * 10'000LL;

        if (SetWaitableTimerEx(
                hRepeatTimer_,
                &liDue,
                0,
                nullptr,
                nullptr,
                nullptr,
                0) == FALSE)
        {
            HandleTimingFailure();
            return;
        }

        ullArmedGeneration_ = effects.ullGeneration;
    }
}

void RepeatRuntime::HandleTimerDue(
    const UINT64 ullGeneration,
    const bool bEffectiveEnabled)
{
    const auto effects = state_.OnTimerDue(ullGeneration, bEffectiveEnabled);

    ApplyEffects(effects);

    if (!effects.bRequestSyntheticKeyDown) return;

    RepeatSessionView session{};
    if (!state_.TryGetCurrentSession(session) ||
        session.ullGeneration != ullGeneration)
    {
        return;
    }

    INPUT inputKeyDown = BuildSyntheticKeyDown(session.dwVirtualKey, session.key);

    SetLastError(ERROR_SUCCESS);
    const UINT uSent =
        SendInput(
            1,
            &inputKeyDown,
            sizeof(inputKeyDown));

    if (uSent != 1)
    {
        ApplyEffects(state_.Stop(StopReason::InjectionFailure));
        return;
    }

    ApplyEffects(state_.OnSyntheticSendSucceeded(ullGeneration));
}

void RepeatRuntime::HandleTimingFailure() noexcept
{
    ApplyEffects(state_.Stop(StopReason::TimerFailure));
    bReady_ = false;
}

} // namespace repeatboost::engine
