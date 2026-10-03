#include "ControlWindowExecution.h"

#include "resource.h"

#include <strsafe.h>

namespace repeatboost
{
ControlWindowExecution::ControlWindowExecution(
    const HINSTANCE hInstance,
    settings::SettingsStore& settingsStore,
    engine::HookExecution& hookExecution,
    engine::SettingsProcessExecution& settingsProcess) noexcept
    : hInstance_(hInstance),
      settingsStore_(settingsStore),
      hookExecution_(hookExecution),
      settingsProcess_(settingsProcess)
{
}

ControlWindowExecution::~ControlWindowExecution()
{
    (void)BeginShutdown();
    if (bClassRegistered_)
        UnregisterClassW(kWindowClassName, hInstance_);
}

bool ControlWindowExecution::Initialize()
{
    uTaskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");
    if (uTaskbarCreatedMessage_ == 0) return false;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = hInstance_;
    wc.lpfnWndProc = &ControlWindowExecution::WindowProc;
    wc.lpszClassName = kWindowClassName;

    if (RegisterClassExW(&wc) == 0) return false;
    bClassRegistered_ = true;

    hWndControl_ = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        kWindowClassName,
        L"RepeatBoost Engine Control",
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        hInstance_,
        this);

    if (hWndControl_ == nullptr) return false;

    bControlWindowReady_ = true;

    if (!AddTrayIconWithRetry())
    {
        (void)BeginShutdown();
        return false;
    }

    hPowerNotification_ = RegisterSuspendResumeNotification(
        hWndControl_,
        DEVICE_NOTIFY_WINDOW_HANDLE);

    TryRegisterWts();
    RecomputeDerivedFacts();
    return true;
}

void ControlWindowExecution::SetConfiguredEnabled(const bool bEnabled) noexcept
{
    facts_.bConfiguredEnabled = bEnabled;
    RecomputeDerivedFacts();
}

void ControlWindowExecution::RefreshExecutionState() noexcept
{
    RecomputeDerivedFacts();
}

void ControlWindowExecution::Shutdown() noexcept
{
    (void)BeginShutdown();
}

const ControlFacts& ControlWindowExecution::Facts() const noexcept
{
    return facts_;
}

PlatformResourceState ControlWindowExecution::PlatformResources() const noexcept
{
    return PlatformResourceState{
        .bControlWindow = hWndControl_ != nullptr,
        .bTrayIcon = bTrayAdded_,
        .bForegroundHook = hookExecution_.ForegroundMonitoringReady(),
        .bPowerNotification = hPowerNotification_ != nullptr,
        .bWtsRegistration = bWtsRegistered_,
        .bSettingsProcess = settingsProcess_.Running(),
    };
}

HWND ControlWindowExecution::WindowHandle() const noexcept
{
    return hWndControl_;
}

LRESULT CALLBACK ControlWindowExecution::WindowProc(
    HWND hWnd,
    const UINT uMessage,
    const WPARAM wParam,
    const LPARAM lParam)
{
    ControlWindowExecution* pSelf = nullptr;

    if (uMessage == WM_NCCREATE)
    {
        const auto* pCreate =
            reinterpret_cast<const CREATESTRUCTW*>(lParam);

        pSelf =
            static_cast<ControlWindowExecution*>(
                pCreate->lpCreateParams);

        SetWindowLongPtrW(
            hWnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(pSelf));

        pSelf->hWndControl_ = hWnd;
    }
    else
    {
        pSelf =
            reinterpret_cast<ControlWindowExecution*>(
                GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (pSelf != nullptr)
        return pSelf->HandleWindowMessage(uMessage, wParam, lParam);

    return DefWindowProcW(hWnd, uMessage, wParam, lParam);
}

LRESULT ControlWindowExecution::HandleWindowMessage(
    const UINT uMessage,
    const WPARAM wParam,
    const LPARAM lParam)
{
    if (uMessage == uTaskbarCreatedMessage_)
    {
        HandleTaskbarCreated();
        return 0;
    }

    switch (uMessage)
    {
    case kTrayCallbackMessage:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU)
            ShowTrayMenu();
        return 0;

    case WM_COMMAND:
        HandleCommand(LOWORD(wParam));
        return 0;

    case kMessageSettingsExited:
        HandleSettingsExited();
        return 0;

    case WM_TIMER:
        if (wParam == kWtsRetryTimerId)
        {
            KillTimer(hWndControl_, kWtsRetryTimerId);
            bWtsRetryPending_ = false;
            TryRegisterWts();
            return 0;
        }
        break;

    case WM_WTSSESSION_CHANGE:
        HandleWtsSessionChange(wParam);
        return 0;

    case WM_POWERBROADCAST:
        HandlePowerBroadcast(wParam);
        return TRUE;

    case WM_QUERYENDSESSION:
        DeactivateHookExecution();
        return TRUE;

    case WM_ENDSESSION:
        if (wParam != FALSE)
            (void)BeginShutdown();
        else
            RecomputeDerivedFacts();
        return 0;

    case WM_DESTROY:
        bControlWindowReady_ = false;
        hWndControl_ = nullptr;
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hWndControl_, uMessage, wParam, lParam);
}

void ControlWindowExecution::HandleCommand(const UINT uCommand)
{
    switch (uCommand)
    {
    case IDM_TRAY_ENABLE:
        if (facts_.bConfiguredEnabled)
        {
            SetConfiguredEnabled(false);
            (void)settingsStore_.PatchEnabled(false);
        }
        else if (settingsStore_.PatchEnabled(true))
        {
            SetConfiguredEnabled(true);
        }
        break;

    case IDM_TRAY_SETTINGS:
        if (!facts_.bShuttingDown)
            (void)settingsProcess_.Launch(
                hWndControl_,
                kMessageSettingsExited);
        break;

    case IDM_TRAY_EXIT:
        (void)BeginShutdown();
        break;

    default:
        break;
    }
}

void ControlWindowExecution::HandleWtsSessionChange(
    const WPARAM wParamEvent)
{
    switch (wParamEvent)
    {
    case WTS_SESSION_LOCK:
        facts_.bSessionAvailable = false;
        RecomputeDerivedFacts();
        break;

    case WTS_SESSION_UNLOCK:
        facts_.bSessionAvailable = true;
        RecomputeDerivedFacts();
        break;

    case WTS_SESSION_LOGOFF:
        facts_.bSessionAvailable = false;
        RecomputeDerivedFacts();
        break;

    default:
        break;
    }
}

void ControlWindowExecution::HandlePowerBroadcast(
    const WPARAM wParamEvent)
{
    switch (wParamEvent)
    {
    case PBT_APMSUSPEND:
        facts_.bPowerAvailable = false;
        RecomputeDerivedFacts();
        break;

    case PBT_APMRESUMEAUTOMATIC:
    case PBT_APMRESUMESUSPEND:
        facts_.bPowerAvailable = true;
        RecomputeDerivedFacts();
        break;

    default:
        break;
    }
}

void ControlWindowExecution::HandleTaskbarCreated()
{
    bTrayAdded_ = false;
    RecomputeDerivedFacts();

    (void)AddTrayIcon();
    RecomputeDerivedFacts();
}

void ControlWindowExecution::HandleSettingsExited()
{
    if (!settingsProcess_.Running()) return;

    settingsProcess_.CompleteObservation();

    const auto settings = settingsStore_.Load();

    if (!hookExecution_.ApplySettings(
            settings.targetPreset,
            engine::TimingSettings{
                .uInitialDelayMs =
                    static_cast<UINT32>(settings.nInitialDelayMs),
                .uRepeatIntervalMs =
                    static_cast<UINT32>(settings.nRepeatIntervalMs),
            }))
    {
        bHookControlReady_ = false;
    }

    facts_.bConfiguredEnabled = settings.bEnabled;
    RecomputeDerivedFacts();
}

void ControlWindowExecution::ShowTrayMenu()
{
    HMENU hRootMenu =
        LoadMenuW(
            hInstance_,
            MAKEINTRESOURCEW(IDR_TRAY_MENU));

    if (hRootMenu == nullptr) return;

    HMENU hMenu = GetSubMenu(hRootMenu, 0);
    if (hMenu == nullptr)
    {
        DestroyMenu(hRootMenu);
        return;
    }

    CheckMenuItem(
        hMenu,
        IDM_TRAY_ENABLE,
        MF_BYCOMMAND |
            (facts_.bConfiguredEnabled ? MF_CHECKED : MF_UNCHECKED));

    POINT pt{};
    if (GetCursorPos(&pt) != FALSE)
    {
        SetForegroundWindow(hWndControl_);

        const UINT uCommand = TrackPopupMenu(
            hMenu,
            TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
            pt.x,
            pt.y,
            0,
            hWndControl_,
            nullptr);

        if (uCommand != 0)
            PostMessageW(hWndControl_, WM_COMMAND, uCommand, 0);
    }

    DestroyMenu(hRootMenu);
}

bool ControlWindowExecution::AddTrayIconWithRetry()
{
    constexpr unsigned int uAttempts = 3;
    for (unsigned int uAttempt = 0; uAttempt < uAttempts; ++uAttempt)
    {
        if (AddTrayIcon()) return true;
        if (uAttempt + 1 < uAttempts) Sleep(100);
    }

    return false;
}

bool ControlWindowExecution::AddTrayIcon()
{
    if (hWndControl_ == nullptr) return false;

    const auto hProductIcon =
        reinterpret_cast<HICON>(
            LoadImageW(
                hInstance_,
                MAKEINTRESOURCEW(IDI_REPEATBOOST),
                IMAGE_ICON,
                0,
                0,
                LR_DEFAULTSIZE | LR_SHARED));

    if (hProductIcon == nullptr) return false;

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWndControl_;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = kTrayCallbackMessage;
    nid.hIcon = hProductIcon;
    if (FAILED(StringCchCopyW(
            nid.szTip,
            ARRAYSIZE(nid.szTip),
            L"RepeatBoost")))
    {
        return false;
    }

    if (Shell_NotifyIconW(NIM_ADD, &nid) == FALSE)
    {
        bTrayAdded_ = false;
        RecomputeDerivedFacts();
        return false;
    }

    bTrayAdded_ = true;
    RecomputeDerivedFacts();
    return true;
}

bool ControlWindowExecution::RemoveTrayIcon()
{
    if (!bTrayAdded_ || hWndControl_ == nullptr) return true;

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWndControl_;
    nid.uID = kTrayIconId;

    const bool bRemoved =
        Shell_NotifyIconW(NIM_DELETE, &nid) != FALSE;

    bTrayAdded_ = false;
    return bRemoved;
}

void ControlWindowExecution::TryRegisterWts()
{
    if (bWtsRegistered_ || hWndControl_ == nullptr) return;

    ++uWtsRegistrationAttempts_;

    if (WTSRegisterSessionNotification(
            hWndControl_,
            NOTIFY_FOR_THIS_SESSION) != FALSE)
    {
        bWtsRegistered_ = true;
        bWtsRetryPending_ = false;
        RecomputeDerivedFacts();
        return;
    }

    const DWORD dwError = GetLastError();
    if (dwError == RPC_S_INVALID_BINDING &&
        uWtsRegistrationAttempts_ < kMaxWtsRegistrationAttempts)
    {
        bWtsRetryPending_ = true;
        SetTimer(
            hWndControl_,
            kWtsRetryTimerId,
            500,
            nullptr);
    }
    else
    {
        bWtsRetryPending_ = false;
    }

    RecomputeDerivedFacts();
}

void ControlWindowExecution::RecomputeDerivedFacts() noexcept
{
    facts_.bRuntimeSafetyReady =
        bControlWindowReady_ &&
        bTrayAdded_ &&
        bWtsRegistered_ &&
        bHookControlReady_ &&
        hookExecution_.Ready() &&
        hookExecution_.ForegroundMonitoringReady() &&
        hPowerNotification_ != nullptr &&
        !facts_.bShuttingDown;

    facts_.bEffectiveEnabled =
        facts_.bConfiguredEnabled &&
        facts_.bRuntimeSafetyReady &&
        facts_.bSessionAvailable &&
        facts_.bPowerAvailable &&
        !facts_.bShuttingDown;

    if (facts_.bEffectiveEnabled == bHookActiveRequested_)
        return;

    const bool bRequestedActive = facts_.bEffectiveEnabled;

    if (hookExecution_.SetActive(bRequestedActive))
    {
        bHookActiveRequested_ = bRequestedActive;
        return;
    }

    bHookControlReady_ = false;
    bHookActiveRequested_ = false;
    facts_.bRuntimeSafetyReady = false;
    facts_.bEffectiveEnabled = false;
}

void ControlWindowExecution::DeactivateHookExecution() noexcept
{
    if (!bHookActiveRequested_) return;

    if (hookExecution_.SetActive(false))
    {
        bHookActiveRequested_ = false;
        return;
    }

    bHookControlReady_ = false;
    bHookActiveRequested_ = false;
    facts_.bRuntimeSafetyReady = false;
    facts_.bEffectiveEnabled = false;
}

bool ControlWindowExecution::BeginShutdown() noexcept
{
    if (facts_.bShuttingDown) return !hookExecution_.Ready();

    facts_.bShuttingDown = true;
    RecomputeDerivedFacts();

    const bool bShutdownPosted =
        hookExecution_.Shutdown();

    if (!bShutdownPosted)
        bHookControlReady_ = false;

    if (hookExecution_.Ready())
    {
        facts_.bShuttingDown = false;
        facts_.bRuntimeSafetyReady = false;
        facts_.bEffectiveEnabled = false;
        return false;
    }

    if (hWndControl_ != nullptr)
        KillTimer(hWndControl_, kWtsRetryTimerId);

    bWtsRetryPending_ = false;
    settingsProcess_.Shutdown();
    CleanupPlatformResources();

    if (hWndControl_ != nullptr)
        DestroyWindow(hWndControl_);

    return true;
}

void ControlWindowExecution::CleanupPlatformResources() noexcept
{
    if (bWtsRegistered_ && hWndControl_ != nullptr)
    {
        (void)WTSUnRegisterSessionNotification(hWndControl_);
        bWtsRegistered_ = false;
    }

    if (hPowerNotification_ != nullptr)
    {
        (void)UnregisterSuspendResumeNotification(hPowerNotification_);
        hPowerNotification_ = nullptr;
    }

    if (bTrayAdded_)
        (void)RemoveTrayIcon();

    RecomputeDerivedFacts();
}
} // namespace repeatboost
