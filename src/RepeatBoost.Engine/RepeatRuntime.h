#pragma once

#include "RepeatState.h"

#include <windows.h>

namespace repeatboost::engine
{
class RepeatRuntime final
{
public:
    RepeatRuntime(
        TargetPreset preset,
        TimingSettings timing) noexcept;
    ~RepeatRuntime();

    RepeatRuntime(const RepeatRuntime&) = delete;
    RepeatRuntime& operator=(const RepeatRuntime&) = delete;

    [[nodiscard]] bool Initialize();
    void Shutdown() noexcept;

    void Stop(StopReason reason) noexcept;
    void ApplySettings(
        TargetPreset preset,
        TimingSettings timing) noexcept;

    [[nodiscard]] HookDecision ProcessInput(
        const InputEvent& input,
        bool bEffectiveEnabled) noexcept;

    [[nodiscard]] HANDLE TimerHandle() const noexcept;
    [[nodiscard]] UINT64 ConsumeTimerSignalGeneration() noexcept;
    void HandleTimerSignaled(
        UINT64 ullGeneration,
        bool bEffectiveEnabled);
    void HandleTimerWaitFailure() noexcept;

    [[nodiscard]] bool Ready() const noexcept;

private:
    RepeatState state_;
    HANDLE hRepeatTimer_{};
    UINT64 ullArmedGeneration_{};
    bool bReady_{};
    bool bShuttingDown_{};

    void ApplyEffects(const TransitionEffects& effects) noexcept;
    void HandleTimerDue(
        UINT64 ullGeneration,
        bool bEffectiveEnabled);
    void HandleTimingFailure() noexcept;
};

} // namespace repeatboost::engine
