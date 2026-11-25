#include "platform/win/ShellActions.h"

#include <windows.h>

#include <shlobj.h>
#include <shobjidl.h>

#include <QDesktopServices>
#include <QDir>
#include <QUrl>

#include <vector>

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

bool showProperties(const QStringList& rawPaths, quintptr owner)
{
    QStringList paths;
    for (const QString& p : rawPaths)
        paths << QDir::toNativeSeparators(p);
    if (paths.isEmpty())
        return false;
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool ok = false;
    if (paths.size() == 1) {
        ok = SHObjectProperties(reinterpret_cast<HWND>(owner), SHOP_FILEPATH,
                                reinterpret_cast<const wchar_t*>(paths.first().utf16()), nullptr) != FALSE;
    } else {
        QList<PIDLIST_ABSOLUTE> pidls;
        for (const QString& p : paths)
            if (PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(reinterpret_cast<const wchar_t*>(p.utf16())))
                pidls << pidl;
        std::vector<PCIDLIST_ABSOLUTE> list(pidls.cbegin(), pidls.cend());
        IShellItemArray* items = nullptr;
        if (!list.empty() && SUCCEEDED(SHCreateShellItemArrayFromIDLists(UINT(list.size()), list.data(), &items))) {
            IDataObject* data = nullptr;
            if (SUCCEEDED(items->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&data)))) {
                ok = SUCCEEDED(SHMultiFileProperties(data, 0));
                data->Release();
            }
            items->Release();
        }
        for (PIDLIST_ABSOLUTE pidl : pidls)
            ILFree(pidl);
    }
    if (SUCCEEDED(init))
        CoUninitialize();
    return ok;
}

} // namespace picazoo::platform
