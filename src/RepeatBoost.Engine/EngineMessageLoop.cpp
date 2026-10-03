#include "EngineMessageLoop.h"

#include <windows.h>

namespace repeatboost::engine
{
int RunEngineMessageLoop()
{
    MSG msg{};

    for (;;)
    {
        const BOOL bResult = GetMessageW(&msg, nullptr, 0, 0);

        if (bResult == -1) return 1;
        if (bResult == 0) return static_cast<int>(msg.wParam);

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}
} // namespace repeatboost::engine
