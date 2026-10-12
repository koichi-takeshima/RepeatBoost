#include "HookExecution.h"

#include <strsafe.h>

namespace repeatboost::engine
{
namespace
{
[[nodiscard]] bool ClassifyLowLevelInput(
    const WPARAM wParam,
    const KBDLLHOOKSTRUCT& kbdEvent,
    InputEvent& input) noexcept
{
    const bool bIsDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    const bool bIsUp = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;

    if (!bIsDown && !bIsUp) return false;

    input = InputEvent{
        .dwVirtualKey = kbdEvent.vkCode,
        .key = KeyIdentity{
            .byScanCode = static_cast<UINT8>(kbdEvent.scanCode),
            .bExtended = (kbdEvent.flags & LLKHF_EXTENDED) != 0,
        },
        .origin =
            (kbdEvent.flags & LLKHF_INJECTED) != 0
                ? InputOrigin::Injected
                : InputOrigin::Physical,
        .action = bIsDown ? KeyAction::Down : KeyAction::Up,
    };
    return true;
}
} // namespace

ForegroundExclusion::ForegroundExclusion(
    const settings::ExcludedApplicationList excluded) noexcept
    : excluded_(excluded)
{
}

ForegroundLookupRequest ForegroundExclusion::Observe(
    const HWND hWindow,
    const DWORD dwProcessId,
    const ULONGLONG ullNowMs,
    const bool bForce) noexcept
{
    if (!bForce &&
        hWindow_ == hWindow &&
        dwProcessId_ == dwProcessId)
    {
        return {};
    }

    hWindow_ = hWindow;
    dwProcessId_ = dwProcessId;
    ++ullGeneration_;
    if (ullGeneration_ == 0) ++ullGeneration_;
    ullPendingSinceMs_ = ullNowMs;
    bExcluded_ = false;
    szBasename_[0] = L'\0';

    // An empty policy needs no process lookup, and unknown HWND/PID is
    // a definitive lookup failure rather than an indefinitely pending one.
    if (excluded_.uCount == 0)
    {
        status_ = ForegroundLookupStatus::Resolved;
        return {};
    }

    if (hWindow == nullptr || dwProcessId == 0)
    {
        status_ = ForegroundLookupStatus::LookupFailed;
        return {};
    }

    status_ = ForegroundLookupStatus::Pending;
    return {
        .hWindow = hWindow,
        .dwProcessId = dwProcessId,
        .ullGeneration = ullGeneration_,
        .bNeeded = true,
    };
}

bool ForegroundExclusion::Complete(
    const HWND hWindow,
    const DWORD dwProcessId,
    const UINT64 ullGeneration,
    const wchar_t* pszBasename) noexcept
{
    if (status_ != ForegroundLookupStatus::Pending ||
        hWindow != hWindow_ ||
        dwProcessId != dwProcessId_ ||
        ullGeneration != ullGeneration_)
    {
        return false;
    }

    status_ = ForegroundLookupStatus::LookupFailed;
    bExcluded_ = false;
    szBasename_[0] = L'\0';

    if (pszBasename == nullptr ||
        pszBasename[0] == L'\0' ||
        FAILED(StringCchCopyW(
            szBasename_,
            ARRAYSIZE(szBasename_),
            pszBasename)))
    {
        return true;
    }

    status_ = ForegroundLookupStatus::Resolved;
    bExcluded_ = excluded_.Contains(szBasename_);
    return true;
}

bool ForegroundExclusion::IsRepeatAllowed(
    const ULONGLONG ullNowMs) noexcept
{
    if (status_ == ForegroundLookupStatus::Pending &&
        ullNowMs - ullPendingSinceMs_ >= kLookupDeadlineMs)
    {
        status_ = ForegroundLookupStatus::LookupFailed;
        bExcluded_ = false;
    }

    return status_ == ForegroundLookupStatus::LookupFailed ||
           (status_ == ForegroundLookupStatus::Resolved &&
            !bExcluded_);
}

void ForegroundExclusion::ApplySettings(
    const settings::ExcludedApplicationList& excluded) noexcept
{
    excluded_ = excluded;
    if (status_ == ForegroundLookupStatus::Resolved)
        bExcluded_ = excluded_.Contains(szBasename_);
}

void ForegroundExclusion::Invalidate() noexcept
{
    hWindow_ = nullptr;
    dwProcessId_ = 0;
    ++ullGeneration_;
    if (ullGeneration_ == 0) ++ullGeneration_;
    status_ = ForegroundLookupStatus::LookupFailed;
    bExcluded_ = false;
    szBasename_[0] = L'\0';
}

ForegroundLookupStatus ForegroundExclusion::Status() const noexcept
{
    return status_;
}

HWND ForegroundExclusion::Window() const noexcept
{
    return hWindow_;
}

DWORD ForegroundExclusion::ProcessId() const noexcept
{
    return dwProcessId_;
}

HookExecution::HookExecution(
    const HINSTANCE hInstance,
    const RepeatSettings settings,
    const settings::ExcludedApplicationList excluded) noexcept
    : hInstance_(hInstance),
      repeatRuntime_(settings),
      foreground_(excluded)
{
}

HookExecution::HookExecution(
    const HINSTANCE hInstance,
    const TargetSettings target,
    const TimingSettings timing) noexcept
    : HookExecution(
          hInstance,
          RepeatSettings{
              .sets = {
                  RepeatSettingSet{
                      .target = target,
                      .timing = timing,
                  },
              },
              .uCount = 1,
          })
{
}

HookExecution::~HookExecution()
{
    (void)Shutdown();
}

bool HookExecution::Initialize()
{
    if (hThread_ != nullptr) return Ready();

    hControlFailureEvent_ =
        CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (hControlFailureEvent_ == nullptr) return false;

    hStartedEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (hStartedEvent_ == nullptr)
    {
        CloseHandle(hControlFailureEvent_);
        hControlFailureEvent_ = nullptr;
        return false;
    }

    hThread_ = CreateThread(
        nullptr,
        0,
        &HookExecution::ThreadProc,
        this,
        0,
        nullptr);

    if (hThread_ == nullptr)
    {
        CloseHandle(hStartedEvent_);
        hStartedEvent_ = nullptr;
        CloseHandle(hControlFailureEvent_);
        hControlFailureEvent_ = nullptr;
        return false;
    }

    HANDLE startupHandles[] = {
        hStartedEvent_,
        hThread_,
    };

    const DWORD dwWaitResult =
        WaitForMultipleObjects(
            ARRAYSIZE(startupHandles),
            startupHandles,
            FALSE,
            INFINITE);

    CloseHandle(hStartedEvent_);
    hStartedEvent_ = nullptr;

    if (dwWaitResult == WAIT_OBJECT_0 && Ready()) return true;

    WaitForSingleObject(hThread_, INFINITE);
    CloseHandle(hThread_);
    hThread_ = nullptr;
    dwThreadId_ = 0;

    CloseHandle(hControlFailureEvent_);
    hControlFailureEvent_ = nullptr;
    return false;
}

bool HookExecution::ApplySettings(
    const RepeatSettings settings) noexcept
{
    return ApplySettings(settings, {});
}

bool HookExecution::ApplySettings(
    const RepeatSettings settings,
    const settings::ExcludedApplicationList& excluded) noexcept
{
    if (hThread_ == nullptr || dwThreadId_ == 0) return false;

    AcquireSRWLockShared(&settingsPostLock_);
    if (!bAcceptingSettingsMessages_)
    {
        ReleaseSRWLockShared(&settingsPostLock_);
        return false;
    }

    auto* pSettingsSnapshot = static_cast<SettingsSnapshot*>(
        HeapAlloc(
            GetProcessHeap(),
            0,
            sizeof(SettingsSnapshot)));
    if (pSettingsSnapshot == nullptr)
    {
        ReleaseSRWLockShared(&settingsPostLock_);
        HandleControlPostFailure();
        return false;
    }

    *pSettingsSnapshot = SettingsSnapshot{
        .repeat = settings,
        .excluded = excluded,
    };
    const bool bPosted =
        PostThreadMessageW(
            dwThreadId_,
            kMessageApplySettings,
            reinterpret_cast<WPARAM>(pSettingsSnapshot),
            0) != FALSE;

    if (!bPosted)
        (void)HeapFree(
            GetProcessHeap(),
            0,
            pSettingsSnapshot);

    ReleaseSRWLockShared(&settingsPostLock_);

    if (bPosted) return true;

    HandleControlPostFailure();
    return false;
}

bool HookExecution::ApplySettings(
    const TargetSettings target,
    const TimingSettings timing) noexcept
{
    return ApplySettings(
        RepeatSettings{
            .sets = {
                RepeatSettingSet{
                    .target = target,
                    .timing = timing,
                },
            },
            .uCount = 1,
        });
}

void HookExecution::SetControlWindow(const HWND hWindow) noexcept
{
    // Assigned on the Control Window thread before Hook thread startup.
    hControlWindow_ = hWindow;
}

void HookExecution::CompleteForegroundLookup(
    const HWND hWindow,
    const DWORD dwProcessId,
    const UINT64 ullGeneration,
    const wchar_t* pszBasename) noexcept
{
    AcquireSRWLockShared(&settingsPostLock_);
    if (!bAcceptingSettingsMessages_ ||
        hThread_ == nullptr ||
        dwThreadId_ == 0)
    {
        ReleaseSRWLockShared(&settingsPostLock_);
        return;
    }

    auto* pResult = static_cast<ForegroundLookupResult*>(
        HeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            sizeof(ForegroundLookupResult)));
    if (pResult == nullptr)
    {
        ReleaseSRWLockShared(&settingsPostLock_);
        // Pending will become LookupFailed at its bounded deadline.
        return;
    }

    pResult->hWindow = hWindow;
    pResult->dwProcessId = dwProcessId;
    pResult->ullGeneration = ullGeneration;
    if (pszBasename != nullptr)
        (void)StringCchCopyW(
            pResult->szBasename,
            ARRAYSIZE(pResult->szBasename),
            pszBasename);

    if (PostThreadMessageW(
            dwThreadId_,
            kMessageForegroundLookupResult,
            reinterpret_cast<WPARAM>(pResult),
            0) == FALSE)
    {
        (void)HeapFree(GetProcessHeap(), 0, pResult);
        // No process-wide safety-gate mutation for a lookup failure.
    }

    ReleaseSRWLockShared(&settingsPostLock_);
}

bool HookExecution::SetActive(const bool bActive) noexcept
{
    if (!bActive)
        InterlockedExchange(&lActiveGate_, FALSE);

    if (hThread_ == nullptr || dwThreadId_ == 0) return false;

    if (PostThreadMessageW(
            dwThreadId_,
            kMessageSetActive,
            bActive ? TRUE : FALSE,
            0) != FALSE)
    {
        return true;
    }

    HandleControlPostFailure();
    return false;
}

bool HookExecution::Shutdown() noexcept
{
    if (hThread_ == nullptr) return true;

    InterlockedExchange(&lActiveGate_, FALSE);

    bool bShutdownRequested =
        WaitForSingleObject(hThread_, 0) == WAIT_OBJECT_0;

    if (!bShutdownRequested)
    {
        bShutdownRequested =
            dwThreadId_ != 0 &&
            PostThreadMessageW(
                dwThreadId_,
                kMessageShutdown,
                0,
                0) != FALSE;

        if (!bShutdownRequested)
            HandleControlPostFailure();
    }

    const DWORD dwWaitResult =
        WaitForSingleObject(hThread_, kShutdownWaitMs);

    if (dwWaitResult != WAIT_OBJECT_0)
        return false;

    CloseHandle(hThread_);
    hThread_ = nullptr;
    dwThreadId_ = 0;

    if (hControlFailureEvent_ != nullptr)
    {
        CloseHandle(hControlFailureEvent_);
        hControlFailureEvent_ = nullptr;
    }

    return bShutdownRequested;
}

bool HookExecution::Ready() const noexcept
{
    return hThread_ != nullptr &&
           WaitForSingleObject(hThread_, 0) == WAIT_TIMEOUT;
}

bool HookExecution::ForegroundMonitoringReady() const noexcept
{
    return Ready();
}

DWORD WINAPI HookExecution::ThreadProc(void* pContext) noexcept
{
    auto* pSelf = static_cast<HookExecution*>(pContext);
    return pSelf != nullptr ? pSelf->Run() : 1;
}

LRESULT CALLBACK HookExecution::LowLevelKeyboardProc(
    const int nCode,
    const WPARAM wParam,
    const LPARAM lParam)
{
    if (pActiveExecution_ == nullptr)
        return CallNextHookEx(nullptr, nCode, wParam, lParam);

    return pActiveExecution_->HandleLowLevelKeyboard(
        nCode,
        wParam,
        lParam);
}

void CALLBACK HookExecution::ForegroundCallback(
    HWINEVENTHOOK,
    DWORD,
    HWND,
    LONG,
    LONG,
    DWORD,
    DWORD)
{
    if (pActiveExecution_ != nullptr)
        pActiveExecution_->HandleForegroundChanged();
}

DWORD HookExecution::Run() noexcept
{
    (void)SetThreadPriority(
        GetCurrentThread(),
        THREAD_PRIORITY_HIGHEST);

    dwThreadId_ = GetCurrentThreadId();

    MSG msg{};
    (void)PeekMessageW(
        &msg,
        nullptr,
        WM_USER,
        WM_USER,
        PM_NOREMOVE);

    if (repeatRuntime_.Initialize())
    {
        pActiveExecution_ = this;

        hKeyboardHook_ = SetWindowsHookExW(
            WH_KEYBOARD_LL,
            &HookExecution::LowLevelKeyboardProc,
            hInstance_,
            0);

        if (hKeyboardHook_ != nullptr)
        {
            hForegroundHook_ = SetWinEventHook(
                EVENT_SYSTEM_FOREGROUND,
                EVENT_SYSTEM_FOREGROUND,
                nullptr,
                &HookExecution::ForegroundCallback,
                0,
                0,
                WINEVENT_OUTOFCONTEXT |
                    WINEVENT_SKIPOWNPROCESS);
        }
    }

    const bool bReady =
        hKeyboardHook_ != nullptr &&
        hForegroundHook_ != nullptr &&
        repeatRuntime_.Ready();

    if (bReady)
    {
        // The initial foreground might never produce a WinEvent.
        RefreshForeground(true);
        AcquireSRWLockExclusive(&settingsPostLock_);
        bAcceptingSettingsMessages_ = true;
        ReleaseSRWLockExclusive(&settingsPostLock_);
    }

    if (!bReady ||
        hStartedEvent_ == nullptr ||
        SetEvent(hStartedEvent_) == FALSE)
    {
        CleanupThreadResources();
        return 1;
    }

    const int nResult = RunMessageLoop();
    CleanupThreadResources();
    return static_cast<DWORD>(nResult);
}

int HookExecution::RunMessageLoop() noexcept
{
    for (;;)
    {
        const HANDLE hRepeatTimer =
            repeatRuntime_.TimerHandle();

        HANDLE handles[] = {
            hControlFailureEvent_,
            hRepeatTimer,
        };

        const DWORD dwHandleCount =
            hRepeatTimer != nullptr ? 2 : 1;

        const DWORD dwWaitResult =
            MsgWaitForMultipleObjectsEx(
                dwHandleCount,
                handles,
                INFINITE,
                QS_ALLINPUT,
                MWMO_INPUTAVAILABLE);

        if (dwWaitResult == WAIT_OBJECT_0)
        {
            HandleControlFailureSignal();
            return 1;
        }

        if (hRepeatTimer != nullptr &&
            dwWaitResult == WAIT_OBJECT_0 + 1)
        {
            const UINT64 ullGeneration =
                repeatRuntime_
                    .ConsumeTimerSignalGeneration();

            int nExitCode{};
            if (!PumpMessages(nExitCode))
                return nExitCode;

            // Check HWND/PID before a previously armed synthetic down.
            // ForegroundChanged Stop preserves the old held-key claim.
            const bool bActive =
                InterlockedCompareExchange(
                    &lActiveGate_,
                    FALSE,
                    FALSE) != FALSE;
            const bool bForegroundAllowed =
                bActive && ForegroundRepeatAllowed();
            repeatRuntime_.HandleTimerSignaled(
                ullGeneration, bForegroundAllowed);

            if (!repeatRuntime_.Ready())
            {
                InterlockedExchange(&lActiveGate_, FALSE);
                return 1;
            }
            continue;
        }

        if (dwWaitResult ==
            WAIT_OBJECT_0 + dwHandleCount)
        {
            int nExitCode{};
            if (!PumpMessages(nExitCode))
                return nExitCode;

            continue;
        }

        repeatRuntime_.HandleTimerWaitFailure();
        InterlockedExchange(&lActiveGate_, FALSE);
        return 1;
    }
}

bool HookExecution::PumpMessages(
    int& nExitCode) noexcept
{
    MSG msg{};
    while (PeekMessageW(
               &msg,
               nullptr,
               0,
               0,
               PM_REMOVE) != FALSE)
    {
        if (msg.message == WM_QUIT)
        {
            nExitCode =
                static_cast<int>(msg.wParam);
            return false;
        }

        if (!HandleThreadMessage(msg))
        {
            nExitCode = 0;
            return false;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return true;
}

bool HookExecution::HandleThreadMessage(
    const MSG& msg) noexcept
{
    switch (msg.message)
    {
    case kMessageApplySettings:
    {
        auto* pSettingsSnapshot =
            reinterpret_cast<SettingsSnapshot*>(
                msg.wParam);
        if (pSettingsSnapshot == nullptr)
        {
            HandleControlFailureSignal();
            return false;
        }

        // Owned by the consuming Hook thread; a single immutable
        // transport payload keeps both sets and exclusions in sync.
        const RepeatSettings settings =
            pSettingsSnapshot->repeat;
        repeatRuntime_.Stop(
            StopReason::SettingsReload);
        foreground_.ApplySettings(
            pSettingsSnapshot->excluded);
        (void)HeapFree(
            GetProcessHeap(),
            0,
            pSettingsSnapshot);
        repeatRuntime_.ApplySettings(settings);
        RefreshForeground(true);
        return true;
    }

    case kMessageForegroundLookupResult:
    {
        auto* pResult = reinterpret_cast<ForegroundLookupResult*>(
            msg.wParam);
        if (pResult != nullptr)
        {
            // The completion may be queued long after a foreground
            // transition or the bounded Pending deadline.
            (void)foreground_.IsRepeatAllowed(GetTickCount64());
            const HWND hCurrent = GetForegroundWindow();
            DWORD dwCurrentProcessId = 0;
            if (hCurrent != nullptr)
                (void)GetWindowThreadProcessId(
                    hCurrent, &dwCurrentProcessId);
            if (hCurrent != foreground_.Window() ||
                dwCurrentProcessId != foreground_.ProcessId())
            {
                RefreshForeground(false);
            }
            else
            {
                (void)foreground_.Complete(
                    pResult->hWindow,
                    pResult->dwProcessId,
                    pResult->ullGeneration,
                    pResult->szBasename);
            }
            (void)HeapFree(GetProcessHeap(), 0, pResult);
        }
        return true;
    }

    case kMessageSetActive:
        if (msg.wParam == FALSE)
        {
            InterlockedExchange(&lActiveGate_, FALSE);
            repeatRuntime_.Stop(
                StopReason::RuntimeSafetyFailure);
            foreground_.Invalidate();
        }
        else
        {
            RefreshForeground(true);
            InterlockedExchange(
                &lActiveGate_,
                repeatRuntime_.Ready() ? TRUE : FALSE);
        }
        return true;

    case kMessageShutdown:
        InterlockedExchange(&lActiveGate_, FALSE);
        repeatRuntime_.Stop(
            StopReason::Exit);
        return false;

    default:
        return true;
    }
}

void HookExecution::HandleControlFailureSignal() noexcept
{
    InterlockedExchange(&lActiveGate_, FALSE);
    repeatRuntime_.Stop(
        StopReason::RuntimeSafetyFailure);
}

void HookExecution::HandleControlPostFailure() noexcept
{
    InterlockedExchange(&lActiveGate_, FALSE);

    if (hControlFailureEvent_ != nullptr)
        (void)SetEvent(hControlFailureEvent_);
}

LRESULT HookExecution::HandleLowLevelKeyboard(
    const int nCode,
    const WPARAM wParam,
    const LPARAM lParam)
{
    if (nCode < 0)
        return CallNextHookEx(
            hKeyboardHook_,
            nCode,
            wParam,
            lParam);

    const auto* pEvent =
        reinterpret_cast<const KBDLLHOOKSTRUCT*>(
            lParam);

    if (pEvent == nullptr)
        return CallNextHookEx(
            hKeyboardHook_,
            nCode,
            wParam,
            lParam);

    InputEvent inputFact{};
    if (!ClassifyLowLevelInput(
            wParam,
            *pEvent,
            inputFact))
    {
        return CallNextHookEx(
            hKeyboardHook_,
            nCode,
            wParam,
            lParam);
    }

    // Physical input and timer processing each check the actual
    // foreground HWND before using a cached per-app policy.
    const bool bActive =
        InterlockedCompareExchange(
            &lActiveGate_,
            FALSE,
            FALSE) != FALSE;
    const bool bForegroundAllowed =
        bActive &&
        (inputFact.origin == InputOrigin::Injected ||
         ForegroundRepeatAllowed());
    const HookDecision decision =
        repeatRuntime_.ProcessInput(
            inputFact, bForegroundAllowed);

    if (!repeatRuntime_.Ready())
    {
        InterlockedExchange(&lActiveGate_, FALSE);
        PostQuitMessage(1);
    }

    if (decision == HookDecision::Suppress)
        return 1;

    return CallNextHookEx(
        hKeyboardHook_,
        nCode,
        wParam,
        lParam);
}

void HookExecution::HandleForegroundChanged() noexcept
{
    RefreshForeground(true);
}

void HookExecution::RefreshForeground(const bool bForce) noexcept
{
    const HWND hCurrent = GetForegroundWindow();
    DWORD dwProcessId = 0;
    if (hCurrent != nullptr)
        (void)GetWindowThreadProcessId(hCurrent, &dwProcessId);

    if (!bForce &&
        foreground_.Window() == hCurrent &&
        foreground_.ProcessId() == dwProcessId)
    {
        return;
    }

    // A changed window invalidates the old session immediately,
    // independently from the duration of the process-name lookup.
    repeatRuntime_.Stop(StopReason::ForegroundChanged);
    const auto request = foreground_.Observe(
        hCurrent, dwProcessId, GetTickCount64(), bForce);
    if (!request.bNeeded) return;

    if (hControlWindow_ == nullptr ||
        PostMessageW(
            hControlWindow_,
            kMessageForegroundLookupRequest,
            static_cast<WPARAM>(request.ullGeneration),
            reinterpret_cast<LPARAM>(request.hWindow)) == FALSE)
    {
        // Failed delivery is a definitive LookupFailed, not Pending.
        (void)foreground_.Complete(
            request.hWindow,
            request.dwProcessId,
            request.ullGeneration,
            nullptr);
    }
}

bool HookExecution::ForegroundRepeatAllowed() noexcept
{
    RefreshForeground(false);
    return foreground_.IsRepeatAllowed(GetTickCount64());
}

void HookExecution::CleanupThreadResources() noexcept
{
    InterlockedExchange(&lActiveGate_, FALSE);

    AcquireSRWLockExclusive(&settingsPostLock_);
    bAcceptingSettingsMessages_ = false;

    MSG settingsMessage{};
    while (PeekMessageW(
               &settingsMessage,
               nullptr,
               kMessageApplySettings,
               kMessageApplySettings,
               PM_REMOVE) != FALSE)
    {
        auto* pSettingsSnapshot =
            reinterpret_cast<SettingsSnapshot*>(
                settingsMessage.wParam);
        if (pSettingsSnapshot != nullptr)
        {
            (void)HeapFree(
                GetProcessHeap(),
                0,
                pSettingsSnapshot);
        }
    }

    MSG lookupResult{};
    while (PeekMessageW(
               &lookupResult,
               nullptr,
               kMessageForegroundLookupResult,
               kMessageForegroundLookupResult,
               PM_REMOVE) != FALSE)
    {
        auto* pResult =
            reinterpret_cast<ForegroundLookupResult*>(lookupResult.wParam);
        if (pResult != nullptr)
            (void)HeapFree(GetProcessHeap(), 0, pResult);
    }

    ReleaseSRWLockExclusive(&settingsPostLock_);

    if (hForegroundHook_ != nullptr)
    {
        (void)UnhookWinEvent(
            hForegroundHook_);
        hForegroundHook_ = nullptr;
    }

    if (hKeyboardHook_ != nullptr)
    {
        (void)UnhookWindowsHookEx(
            hKeyboardHook_);
        hKeyboardHook_ = nullptr;
    }

    if (pActiveExecution_ == this)
        pActiveExecution_ = nullptr;

    repeatRuntime_.Shutdown();
}
} // namespace repeatboost::engine
