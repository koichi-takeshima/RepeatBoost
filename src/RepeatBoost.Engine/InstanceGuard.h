#pragma once

#include <windows.h>

namespace repeatboost::engine
{
class InstanceGuard final
{
public:
    explicit InstanceGuard(const wchar_t* pszName = L"RepeatBoost.Engine.Instance")
    {
        hMutex_ = CreateMutexW(nullptr, FALSE, pszName);
        if (hMutex_ == nullptr) return;

        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            CloseHandle(hMutex_);
            hMutex_ = nullptr;
            return;
        }

        bAcquired_ = true;
    }

    InstanceGuard(const InstanceGuard&) = delete;
    InstanceGuard& operator=(const InstanceGuard&) = delete;

    ~InstanceGuard()
    {
        if (hMutex_ != nullptr)
        {
            CloseHandle(hMutex_);
        }
    }

    [[nodiscard]] bool Acquired() const noexcept
    {
        return bAcquired_;
    }

private:
    HANDLE hMutex_ = nullptr;
    bool bAcquired_ = false;
};
} // namespace repeatboost::engine
