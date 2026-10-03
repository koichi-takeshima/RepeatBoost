#include "EngineControlPlane.h"
#include "EngineMessageLoop.h"
#include "InstanceGuard.h"
#include "SettingsPersistence.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    repeatboost::engine::InstanceGuard instanceGuard;
    if (!instanceGuard.Acquired()) return 0;

    (void)SetPriorityClass(
        GetCurrentProcess(),
        HIGH_PRIORITY_CLASS);

    wchar_t szSettingsPath[MAX_PATH]{};
    (void)repeatboost::settings::SettingsStore::DefaultSettingsPath(szSettingsPath, ARRAYSIZE(szSettingsPath));

    repeatboost::settings::SettingsStore settingsStore(szSettingsPath);

    const auto settings = settingsStore.Load();

    repeatboost::EngineControlPlane controlPlane(
        hInstance,
        settingsStore,
        settings.targetPreset,
        repeatboost::engine::TimingSettings{
            .uInitialDelayMs = static_cast<UINT32>(settings.nInitialDelayMs),
            .uRepeatIntervalMs = static_cast<UINT32>(settings.nRepeatIntervalMs),
        });

    controlPlane.SetConfiguredEnabled(settings.bEnabled);

    if (!controlPlane.Initialize()) return 4;

    const int nResult = repeatboost::engine::RunEngineMessageLoop();

    controlPlane.Shutdown();
    return nResult;
}
