#pragma once

#include "RepeatRuntime.h"

#include <windows.h>

namespace repeatboost::engine
{
class HookExecution final
{
public:
    HookExecution(
        HINSTANCE hInstance,
        RepeatSettings settings) noexcept;
    HookExecution(
        HINSTANCE hInstance,
        TargetSettings target,
        TimingSettings timing) noexcept;
    ~HookExecution();

    HookExecution(const HookExecution&) = delete;
    HookExecution& operator=(const HookExecution&) = delete;

    [[nodiscard]] bool Initialize();
    [[nodiscard]] bool ApplySettings(RepeatSettings settings) noexcept;
    [[nodiscard]] bool ApplySettings(
        TargetSettings target,
        TimingSettings timing) noexcept;
    [[nodiscard]] bool SetActive(bool bActive) noexcept;
    [[nodiscard]] bool Shutdown() noexcept;

    [[nodiscard]] bool Ready() const noexcept;
    [[nodiscard]] bool ForegroundMonitoringReady() const noexcept;

private:
    static constexpr UINT kMessageApplySettings = WM_APP + 30;
    static constexpr UINT kMessageSetActive = WM_APP + 31;
    static constexpr UINT kMessageShutdown = WM_APP + 32;
    static constexpr DWORD kShutdownWaitMs = 5000;

    inline static HookExecution* pActiveExecution_ = nullptr;

    HINSTANCE hInstance_{};
    HANDLE hThread_{};
    HANDLE hStartedEvent_{};
    HANDLE hControlFailureEvent_{};
    DWORD dwThreadId_{};
    HHOOK hKeyboardHook_{};
    HWINEVENTHOOK hForegroundHook_{};

    RepeatRuntime repeatRuntime_;
    SRWLOCK settingsPostLock_ = SRWLOCK_INIT;
    bool bAcceptingSettingsMessages_{};
    volatile LONG lActiveGate_{};

    static DWORD WINAPI ThreadProc(void* pContext) noexcept;
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static void CALLBACK ForegroundCallback(
        HWINEVENTHOOK hWinEventHook,
        DWORD dwEvent,
        HWND hWnd,
        LONG idObject,
        LONG idChild,
        DWORD dwEventThread,
        DWORD dwmsEventTime);

    DWORD Run() noexcept;
    int RunMessageLoop() noexcept;
    bool PumpMessages(int& nExitCode) noexcept;
    bool HandleThreadMessage(const MSG& msg) noexcept;
    void HandleControlFailureSignal() noexcept;
    void HandleControlPostFailure() noexcept;
    LRESULT HandleLowLevelKeyboard(int nCode, WPARAM wParam, LPARAM lParam);
    void HandleForegroundChanged() noexcept;
    void CleanupThreadResources() noexcept;
};
} // namespace repeatboost::engine
