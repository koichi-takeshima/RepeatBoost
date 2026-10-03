#include <windows.h>
#include <process.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPreviousInstance, PWSTR pszCommandLine, int nCmdShow);

extern "C" void WINAPI RawEntryPoint()
{
    __security_init_cookie();

    const HINSTANCE hInstance = GetModuleHandleW(nullptr);
    if (hInstance == nullptr) ExitProcess(5);

    const int nResult =
        wWinMain(hInstance, nullptr, nullptr, SW_SHOWNORMAL);

    ExitProcess(static_cast<UINT>(nResult));
}
