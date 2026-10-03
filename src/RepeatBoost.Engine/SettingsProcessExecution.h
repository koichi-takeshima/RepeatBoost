#pragma once

#include <windows.h>

namespace repeatboost::engine
{
class SettingsProcessExecution final
{
public:
    SettingsProcessExecution() = default;
    ~SettingsProcessExecution();

    SettingsProcessExecution(const SettingsProcessExecution&) = delete;
    SettingsProcessExecution& operator=(const SettingsProcessExecution&) = delete;

    [[nodiscard]] bool Launch(HWND hWndNotify, UINT uCompletionMessage);
    void CompleteObservation() noexcept;
    void Shutdown() noexcept;

    [[nodiscard]] bool Running() const noexcept;

private:
    HANDLE hProcess_{};
    HANDLE hWait_{};
    HWND hWndNotify_{};
    UINT uCompletionMessage_{};

    static void CALLBACK WaitCallback(void* pContext, BOOLEAN bTimerOrWaitFired);
};
} // namespace repeatboost::engine
