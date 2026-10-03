#pragma once

#include "HookExecution.h"
#include "SettingsPersistence.h"
#include "SettingsProcessExecution.h"

#include <windows.h>
#include <dbt.h>
#include <wtsapi32.h>
#include <shellapi.h>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Wtsapi32.lib")

namespace repeatboost
{
struct ControlFacts
{
    bool bConfiguredEnabled = false;
    bool bRuntimeSafetyReady = false;
    bool bSessionAvailable = true;
    bool bPowerAvailable = true;
    bool bShuttingDown = false;
    bool bEffectiveEnabled = false;
};

struct PlatformResourceState
{
    bool bControlWindow = false;
    bool bTrayIcon = false;
    bool bForegroundHook = false;
    bool bPowerNotification = false;
    bool bWtsRegistration = false;
    bool bSettingsProcess = false;
};

class ControlWindowExecution final
{
public:
    ControlWindowExecution(
        HINSTANCE hInstance,
        settings::SettingsStore& settingsStore,
        engine::HookExecution& hookExecution,
        engine::SettingsProcessExecution& settingsProcess) noexcept;

    ~ControlWindowExecution();

    ControlWindowExecution(const ControlWindowExecution&) = delete;
    ControlWindowExecution& operator=(const ControlWindowExecution&) = delete;

    [[nodiscard]] bool Initialize();
    void SetConfiguredEnabled(bool bEnabled) noexcept;
    void RefreshExecutionState() noexcept;
    void Shutdown() noexcept;

    [[nodiscard]] const ControlFacts& Facts() const noexcept;
    [[nodiscard]] PlatformResourceState PlatformResources() const noexcept;
    [[nodiscard]] HWND WindowHandle() const noexcept;

private:
    static constexpr wchar_t kWindowClassName[] = L"RepeatBoost.Engine.ControlWindow";
    static constexpr UINT kTrayIconId = 1;
    static constexpr UINT kTrayCallbackMessage = WM_APP + 1;
    static constexpr UINT kMessageSettingsExited = WM_APP + 3;
    static constexpr UINT_PTR kWtsRetryTimerId = 10;
    static constexpr unsigned int kMaxWtsRegistrationAttempts = 10;

    HINSTANCE hInstance_{};
    HWND hWndControl_{};
    HPOWERNOTIFY hPowerNotification_{};
    bool bWtsRegistered_{};
    bool bWtsRetryPending_{};
    unsigned int uWtsRegistrationAttempts_{};
    bool bTrayAdded_{};
    bool bClassRegistered_{};
    bool bControlWindowReady_{};
    bool bHookActiveRequested_{};
    bool bHookControlReady_{true};

    UINT uTaskbarCreatedMessage_{};
    ControlFacts facts_{};
    settings::SettingsStore& settingsStore_;
    engine::HookExecution& hookExecution_;
    engine::SettingsProcessExecution& settingsProcess_;

    static LRESULT CALLBACK WindowProc(
        HWND hWnd,
        UINT uMessage,
        WPARAM wParam,
        LPARAM lParam);

    LRESULT HandleWindowMessage(UINT uMessage, WPARAM wParam, LPARAM lParam);
    void HandleCommand(UINT uCommand);
    void HandleWtsSessionChange(WPARAM wParamEvent);
    void HandlePowerBroadcast(WPARAM wParamEvent);
    void HandleTaskbarCreated();
    void HandleSettingsExited();
    void ShowTrayMenu();

    [[nodiscard]] bool AddTrayIconWithRetry();
    [[nodiscard]] bool AddTrayIcon();
    [[nodiscard]] bool RemoveTrayIcon();
    void TryRegisterWts();
    void RecomputeDerivedFacts() noexcept;
    void DeactivateHookExecution() noexcept;
    [[nodiscard]] bool BeginShutdown() noexcept;
    void CleanupPlatformResources() noexcept;
};
} // namespace repeatboost
