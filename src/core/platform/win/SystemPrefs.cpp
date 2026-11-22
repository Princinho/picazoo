#include "platform/win/SystemPrefs.h"

#include <windows.h>

namespace picazoo::platform {

bool animationsEnabled()
{
    BOOL enabled = TRUE;
    if (!SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0))
        return true;
    return enabled != FALSE;
}

bool transparencyEnabled()
{
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS st = RegGetValueW(HKEY_CURRENT_USER,
                                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                                    L"EnableTransparency", RRF_RT_REG_DWORD, nullptr, &value, &size);
    return st != ERROR_SUCCESS || value != 0;
}

} // namespace picazoo::platform
