#include "platform/win/ExplorerOrder.h"

#include <windows.h>

// WIN32_LEAN_AND_MEAN écarte OLE, dont exdisp.h a besoin.
#include <ole2.h>
#include <oleauto.h>

#include <exdisp.h>
#include <shlguid.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <QDir>

using Microsoft::WRL::ComPtr;

namespace picazoo::platform {

namespace {

QString normalized(const QString& path)
{
    QString p = QDir::toNativeSeparators(QDir::cleanPath(path));
    while (p.size() > 3 && p.endsWith(u'\\'))
        p.chop(1);
    return p.toCaseFolded();
}

ComPtr<IFolderView2> folderViewOf(IDispatch* disp, HWND* top)
{
    ComPtr<IServiceProvider> sp;
    if (!disp || FAILED(disp->QueryInterface(IID_PPV_ARGS(&sp))))
        return nullptr;
    ComPtr<IShellBrowser> browser;
    if (FAILED(sp->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&browser))))
        return nullptr;
    ComPtr<IShellView> view;
    if (FAILED(browser->QueryActiveShellView(&view)) || !view)
        return nullptr;
    ComPtr<IFolderView2> fv;
    if (FAILED(view.As(&fv)))
        return nullptr;
    if (top) {
        *top = nullptr;
        ComPtr<IWebBrowserApp> app;
        SHANDLE_PTR h = 0;
        if (SUCCEEDED(disp->QueryInterface(IID_PPV_ARGS(&app))) && SUCCEEDED(app->get_HWND(&h)))
            *top = reinterpret_cast<HWND>(h);
        else
            browser->GetWindow(top);
    }
    return fv;
}

QString folderPathOf(IFolderView2* fv)
{
    ComPtr<IPersistFolder2> pf;
    if (FAILED(fv->GetFolder(IID_PPV_ARGS(&pf))))
        return {};
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (FAILED(pf->GetCurFolder(&pidl)) || !pidl)
        return {};
    QString out;
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetNameFromIDList(pidl, SIGDN_FILESYSPATH, &path)) && path) {
        out = QString::fromWCharArray(path);
        CoTaskMemFree(path);
    }
    CoTaskMemFree(pidl);
    return out;
}

bool viewVisible(IFolderView2* fv)
{
    ComPtr<IShellView> sv;
    if (FAILED(fv->QueryInterface(IID_PPV_ARGS(&sv))))
        return true;
    HWND h = nullptr;
    return SUCCEEDED(sv->GetWindow(&h)) && h && IsWindowVisible(h);
}

QStringList viewOrder(IFolderView2* fv)
{
    QStringList names;
    ComPtr<IShellItemArray> items;
    if (FAILED(fv->Items(SVGIO_ALLVIEW | SVGIO_FLAG_VIEWORDER, IID_PPV_ARGS(&items))) || !items)
        return names;
    DWORD n = 0;
    if (FAILED(items->GetCount(&n)))
        return names;
    names.reserve(int(n));
    for (DWORD i = 0; i < n; ++i) {
        ComPtr<IShellItem> item;
        if (FAILED(items->GetItemAt(i, &item)) || !item)
            continue;
        PWSTR name = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_PARENTRELATIVEPARSING, &name)) && name) {
            names.push_back(QString::fromWCharArray(name));
            CoTaskMemFree(name);
        }
    }
    return names;
}

struct Candidate {
    ComPtr<IFolderView2> view;
    int score = -1;
};

void consider(Candidate& best, ComPtr<IFolderView2> fv, HWND top, const QString& wanted, HWND fg)
{
    if (!fv || normalized(folderPathOf(fv.Get())) != wanted)
        return;
    int score = 0;
    if (top && fg && (top == fg || GetAncestor(fg, GA_ROOT) == top))
        score += 2;
    if (viewVisible(fv.Get()))
        score += 1;
    if (score > best.score) {
        best.view = std::move(fv);
        best.score = score;
    }
}

} // namespace

std::optional<QStringList> explorerViewOrder(const QString& folder, quintptr foreground)
{
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    std::optional<QStringList> result;
    {
        const QString wanted = normalized(folder);
        const HWND fg = reinterpret_cast<HWND>(foreground);
        ComPtr<IShellWindows> windows;
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&windows)))) {
            Candidate best;
            long count = 0;
            windows->get_Count(&count);
            for (long i = 0; i < count; ++i) {
                VARIANT v;
                VariantInit(&v);
                v.vt = VT_I4;
                v.lVal = i;
                ComPtr<IDispatch> disp;
                if (FAILED(windows->Item(v, &disp)) || !disp)
                    continue;
                HWND top = nullptr;
                consider(best, folderViewOf(disp.Get(), &top), top, wanted, fg);
            }
            // Le bureau n'est pas dans l'énumération ordinaire.
            VARIANT loc, empty;
            VariantInit(&loc);
            VariantInit(&empty);
            loc.vt = VT_I4;
            loc.lVal = CSIDL_DESKTOP;
            long hwnd = 0;
            ComPtr<IDispatch> desk;
            if (SUCCEEDED(windows->FindWindowSW(&loc, &empty, SWC_DESKTOP, &hwnd, SWFO_NEEDDISPATCH, &desk)) && desk) {
                HWND top = nullptr;
                ComPtr<IFolderView2> fv = folderViewOf(desk.Get(), &top);
                wchar_t cls[64] = {};
                const bool fgIsDesktop = fg && GetClassNameW(fg, cls, 64) &&
                                         (wcscmp(cls, L"Progman") == 0 || wcscmp(cls, L"WorkerW") == 0);
                consider(best, std::move(fv), fgIsDesktop ? fg : top, wanted, fg);
            }
            if (best.view)
                result = viewOrder(best.view.Get());
        }
    }
    if (SUCCEEDED(init))
        CoUninitialize();
    if (result && result->isEmpty())
        result.reset();
    return result;
}

} // namespace picazoo::platform
