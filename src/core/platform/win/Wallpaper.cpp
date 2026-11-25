#include "platform/win/Wallpaper.h"

#include "platform/win/Paths.h"

#include <windows.h>
#include <shobjidl.h>

#include <QFileInfo>

namespace picazoo::platform {

namespace {

// COM sur le fil appelant, le temps d'un appel.
struct ComScope {
    HRESULT hr;
    ComScope() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}
    ~ComScope()
    {
        if (SUCCEEDED(hr))
            CoUninitialize();
    }
};

IDesktopWallpaper* createWallpaper()
{
    IDesktopWallpaper* w = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(DesktopWallpaper), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&w))))
        return nullptr;
    return w;
}

QString fromCoTask(LPWSTR s)
{
    if (!s)
        return {};
    QString out = QString::fromWCharArray(s);
    CoTaskMemFree(s);
    return out;
}

} // namespace

bool wallpaperSupported(const QString& path)
{
    static const QStringList ok{QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("jfif"),
                                QStringLiteral("png"), QStringLiteral("bmp"),  QStringLiteral("gif"),
                                QStringLiteral("tif"), QStringLiteral("tiff")};
    return ok.contains(QFileInfo(path).suffix().toLower());
}

std::optional<WallpaperSnapshot> snapshotWallpaper()
{
    ComScope com;
    IDesktopWallpaper* w = createWallpaper();
    if (!w)
        return std::nullopt;
    WallpaperSnapshot snap;
    snap.restorable = true;
    UINT count = 0;
    if (SUCCEEDED(w->GetMonitorDevicePathCount(&count))) {
        for (UINT i = 0; i < count; ++i) {
            LPWSTR id = nullptr;
            if (FAILED(w->GetMonitorDevicePathAt(i, &id)))
                continue;
            const QString monitor = fromCoTask(id);
            LPWSTR file = nullptr;
            QString path;
            if (SUCCEEDED(w->GetWallpaper(platform::toWin32(monitor).c_str(), &file)))
                path = fromCoTask(file);
            // Un écran sans image (couleur unie) ou dont l'image a disparu ne peut pas être rétabli.
            if (path.isEmpty() || !QFileInfo::exists(path))
                snap.restorable = false;
            snap.monitors << qMakePair(monitor, path);
        }
    }
    DESKTOP_WALLPAPER_POSITION pos = DWPOS_FILL;
    if (SUCCEEDED(w->GetPosition(&pos)))
        snap.position = int(pos);
    DESKTOP_SLIDESHOW_STATE state{};
    if (SUCCEEDED(w->GetStatus(&state)) && (state & DSS_ENABLED) != 0) {
        snap.slideshow = true;
        snap.restorable = false;
    }
    if (snap.monitors.isEmpty())
        snap.restorable = false;
    w->Release();
    return snap;
}

bool setWallpaper(const QString& path)
{
    if (!wallpaperSupported(path) || !QFileInfo::exists(path))
        return false;
    ComScope com;
    IDesktopWallpaper* w = createWallpaper();
    if (!w)
        return false;
    const HRESULT hr = w->SetWallpaper(nullptr, platform::toWin32(path).c_str());
    w->Release();
    return SUCCEEDED(hr);
}

bool restoreWallpaper(const WallpaperSnapshot& snapshot)
{
    if (!snapshot.restorable)
        return false;
    ComScope com;
    IDesktopWallpaper* w = createWallpaper();
    if (!w)
        return false;
    bool ok = true;
    for (const auto& m : snapshot.monitors)
        ok &= SUCCEEDED(w->SetWallpaper(platform::toWin32(m.first).c_str(), platform::toWin32(m.second).c_str()));
    w->SetPosition(DESKTOP_WALLPAPER_POSITION(snapshot.position));
    w->Release();
    return ok;
}

} // namespace picazoo::platform
