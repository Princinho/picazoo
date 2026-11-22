#include "platform/win/ShellActions.h"

#include <windows.h>

#include <shlobj.h>

#include <QDesktopServices>
#include <QUrl>

namespace picazoo::platform {

bool revealInExplorer(const QString& path)
{
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(reinterpret_cast<const wchar_t*>(path.utf16()));
    bool ok = false;
    if (pidl) {
        ok = SUCCEEDED(SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0));
        ILFree(pidl);
    }
    if (SUCCEEDED(init))
        CoUninitialize();
    return ok;
}

bool openWithDefaultApp(const QString& path)
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

} // namespace picazoo::platform
