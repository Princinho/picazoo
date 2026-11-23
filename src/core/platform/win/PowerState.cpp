#include "platform/win/PowerState.h"

#include <windows.h>

namespace picazoo::platform {

bool onBatteryOrSaver()
{
    SYSTEM_POWER_STATUS s{};
    if (!GetSystemPowerStatus(&s))
        return false;
    if (s.SystemStatusFlag == 1) // économiseur d'énergie
        return true;
    return s.ACLineStatus == 0; // 1 : secteur, 255 : inconnu (poste fixe)
}

void enterBackgroundMode()
{
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
}

} // namespace picazoo::platform
