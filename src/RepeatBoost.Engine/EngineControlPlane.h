#pragma once

#include "ControlWindowExecution.h"
#include "HookExecution.h"
#include "SettingsPersistence.h"
#include "SettingsProcessExecution.h"

namespace repeatboost
{
class EngineControlPlane final
{
public:
    EngineControlPlane(
        HINSTANCE hInstance,
        settings::SettingsStore& settingsStore,
        const engine::RepeatSettings settings) noexcept
        : hookExecution_(hInstance, settings),
          controlWindow_(
              hInstance,
              settingsStore,
              hookExecution_,
              settingsProcess_)
    {
    }

    EngineControlPlane(
        HINSTANCE hInstance,
        settings::SettingsStore& settingsStore,
        const engine::TargetSettings target,
        const engine::TimingSettings timing) noexcept
        : hookExecution_(hInstance, target, timing),
          controlWindow_(
              hInstance,
              settingsStore,
              hookExecution_,
              settingsProcess_)
    {
    }

    EngineControlPlane(const EngineControlPlane&) = delete;
    EngineControlPlane& operator=(const EngineControlPlane&) = delete;

    ~EngineControlPlane()
    {
        Shutdown();
    }

    [[nodiscard]] bool Initialize()
    {
        if (!controlWindow_.Initialize()) return false;

        if (!hookExecution_.Initialize())
        {
            controlWindow_.Shutdown();
            return false;
        }

        controlWindow_.RefreshExecutionState();
        return true;
    }

    void SetConfiguredEnabled(const bool bEnabled) noexcept
    {
        controlWindow_.SetConfiguredEnabled(bEnabled);
    }

    void Shutdown() noexcept
    {
        controlWindow_.Shutdown();
    }

    [[nodiscard]] const ControlFacts& Facts() const noexcept
    {
        return controlWindow_.Facts();
    }

    [[nodiscard]] PlatformResourceState PlatformResources() const noexcept
    {
        return controlWindow_.PlatformResources();
    }

    [[nodiscard]] HWND WindowHandle() const noexcept
    {
        return controlWindow_.WindowHandle();
    }

    [[nodiscard]] bool RepeatRuntimeReady() const noexcept
    {
        return hookExecution_.Ready();
    }

private:
    engine::HookExecution hookExecution_;
    engine::SettingsProcessExecution settingsProcess_;
    ControlWindowExecution controlWindow_;
};
} // namespace repeatboost
