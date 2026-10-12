#pragma once

#include "RepeatRuntime.h"
#include "SettingsPersistence.h"

#include <windows.h>

namespace repeatboost::engine
{
// The Hook thread owns this interpretation of externally acquired foreground
// facts. Window/process queries themselves remain on the Control Window thread.
enum class ForegroundLookupStatus
{
    Pending,
    Resolved,
    LookupFailed,
};

struct ForegroundLookupRequest
{
    HWND hWindow{};
    DWORD dwProcessId{};
    UINT64 ullGeneration{};
    bool bNeeded{};
};

class ForegroundExclusion final
{
public:
    explicit ForegroundExclusion(
        settings::ExcludedApplicationList excluded = {}) noexcept;

    [[nodiscard]] ForegroundLookupRequest Observe(
        HWND hWindow,
        DWORD dwProcessId,
        ULONGLONG ullNowMs,
        bool bForce = false) noexcept;
    [[nodiscard]] bool Complete(
        HWND hWindow,
        DWORD dwProcessId,
        UINT64 ullGeneration,
        const wchar_t* pszBasename) noexcept;
    [[nodiscard]] bool IsRepeatAllowed(ULONGLONG ullNowMs) noexcept;
    void ApplySettings(const settings::ExcludedApplicationList& excluded) noexcept;
    void Invalidate() noexcept;

    [[nodiscard]] ForegroundLookupStatus Status() const noexcept;
    [[nodiscard]] HWND Window() const noexcept;
    [[nodiscard]] DWORD ProcessId() const noexcept;

    // A delayed Control Window lookup cannot pin input indefinitely.
    // Time elapsed is checked on fresh input or timer/message wake; never
    // block a low-level keyboard hook waiting for the lookup.
    static constexpr ULONGLONG kLookupDeadlineMs = 200;

private:
    settings::ExcludedApplicationList excluded_{};
    HWND hWindow_{};
    DWORD dwProcessId_{};
    UINT64 ullGeneration_{};
    ULONGLONG ullPendingSinceMs_{};
    ForegroundLookupStatus status_{ForegroundLookupStatus::LookupFailed};
    bool bExcluded_{};
    wchar_t szBasename_[settings::kExcludedApplicationNameCapacity]{};
};

class HookExecution final
{
public:
    HookExecution(
        HINSTANCE hInstance,
        RepeatSettings settings,
        settings::ExcludedApplicationList excluded = {}) noexcept;
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
        RepeatSettings settings,
        const settings::ExcludedApplicationList& excluded) noexcept;
    void SetControlWindow(HWND hWindow) noexcept;
    void CompleteForegroundLookup(
        HWND hWindow,
        DWORD dwProcessId,
        UINT64 ullGeneration,
        const wchar_t* pszBasename) noexcept;
    static constexpr UINT kMessageForegroundLookupRequest = WM_APP + 40;
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
    static constexpr UINT kMessageForegroundLookupResult = WM_APP + 41;
    static constexpr DWORD kShutdownWaitMs = 5000;

    inline static HookExecution* pActiveExecution_ = nullptr;

    HINSTANCE hInstance_{};
    HANDLE hThread_{};
    HANDLE hStartedEvent_{};
    HANDLE hControlFailureEvent_{};
    DWORD dwThreadId_{};
    HHOOK hKeyboardHook_{};
    HWINEVENTHOOK hForegroundHook_{};

    struct SettingsSnapshot
    {
        RepeatSettings repeat{};
        settings::ExcludedApplicationList excluded{};
    };

    struct ForegroundLookupResult
    {
        HWND hWindow{};
        DWORD dwProcessId{};
        UINT64 ullGeneration{};
        wchar_t szBasename[settings::kExcludedApplicationNameCapacity]{};
    };

    RepeatRuntime repeatRuntime_;
    ForegroundExclusion foreground_;
    HWND hControlWindow_{};
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
    void RefreshForeground(bool bForce) noexcept;
    [[nodiscard]] bool ForegroundRepeatAllowed() noexcept;
    void CleanupThreadResources() noexcept;
};
} // namespace repeatboost::engine
