#include "SettingsProcessExecution.h"

#include <pathcch.h>

#pragma comment(lib, "Pathcch.lib")

namespace repeatboost::engine
{
SettingsProcessExecution::~SettingsProcessExecution()
{
    Shutdown();
}

bool SettingsProcessExecution::Launch(
    const HWND hWndNotify,
    const UINT uCompletionMessage)
{
    if (hProcess_ != nullptr) return true;

    wchar_t szEngineDirectory[MAX_PATH]{};
    const DWORD dwExecutablePathLength =
        GetModuleFileNameW(
            nullptr,
            szEngineDirectory,
            ARRAYSIZE(szEngineDirectory));

    if (dwExecutablePathLength == 0 ||
        dwExecutablePathLength >= ARRAYSIZE(szEngineDirectory))
    {
        return false;
    }

    if (FAILED(PathCchRemoveFileSpec(
            szEngineDirectory,
            ARRAYSIZE(szEngineDirectory))))
    {
        return false;
    }

    wchar_t szSettingsDirectory[MAX_PATH]{};
    if (FAILED(PathCchCombine(
            szSettingsDirectory,
            ARRAYSIZE(szSettingsDirectory),
            szEngineDirectory,
            L"settings")))
    {
        return false;
    }

    wchar_t szLaunchPath[MAX_PATH]{};
    if (FAILED(PathCchCombine(
            szLaunchPath,
            ARRAYSIZE(szLaunchPath),
            szSettingsDirectory,
            L"RepeatBoost.Settings.exe")))
    {
        return false;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    if (CreateProcessW(
            szLaunchPath,
            nullptr,
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            szSettingsDirectory,
            &si,
            &pi) == FALSE)
    {
        return false;
    }

    CloseHandle(pi.hThread);
    hProcess_ = pi.hProcess;
    hWndNotify_ = hWndNotify;
    uCompletionMessage_ = uCompletionMessage;

    if (RegisterWaitForSingleObject(
            &hWait_,
            hProcess_,
            &SettingsProcessExecution::WaitCallback,
            this,
            INFINITE,
            WT_EXECUTEONLYONCE) != FALSE)
    {
        return true;
    }

    CloseHandle(hProcess_);
    hProcess_ = nullptr;
    hWndNotify_ = nullptr;
    uCompletionMessage_ = 0;
    hWait_ = nullptr;
    return false;
}

void SettingsProcessExecution::CompleteObservation() noexcept
{
    if (hWait_ != nullptr)
    {
        UnregisterWaitEx(hWait_, INVALID_HANDLE_VALUE);
        hWait_ = nullptr;
    }

    if (hProcess_ != nullptr)
    {
        CloseHandle(hProcess_);
        hProcess_ = nullptr;
    }

    hWndNotify_ = nullptr;
    uCompletionMessage_ = 0;
}

void SettingsProcessExecution::Shutdown() noexcept
{
    CompleteObservation();
}

bool SettingsProcessExecution::Running() const noexcept
{
    return hProcess_ != nullptr;
}

void CALLBACK SettingsProcessExecution::WaitCallback(
    void* pContext,
    BOOLEAN)
{
    auto* pSelf = static_cast<SettingsProcessExecution*>(pContext);
    if (pSelf == nullptr ||
        pSelf->hWndNotify_ == nullptr ||
        pSelf->uCompletionMessage_ == 0)
    {
        return;
    }

    (void)PostMessageW(
        pSelf->hWndNotify_,
        pSelf->uCompletionMessage_,
        0,
        0);
}
} // namespace repeatboost::engine
