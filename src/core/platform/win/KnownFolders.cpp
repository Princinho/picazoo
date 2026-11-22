#include "platform/win/KnownFolders.h"

#include "platform/win/Paths.h"

#include <windows.h>

#include <knownfolders.h>
#include <shlobj.h>

namespace picazoo::platform {

namespace {

QString knownFolder(REFKNOWNFOLDERID id)
{
    PWSTR path = nullptr;
    QString out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &path)))
        out = normalizePath(QString::fromWCharArray(path));
    CoTaskMemFree(path);
    return out;
}

} // namespace

QStringList defaultPictureFolders()
{
    QStringList out;
    for (const QString& p : {knownFolder(FOLDERID_Pictures), knownFolder(FOLDERID_Desktop),
                             knownFolder(FOLDERID_Documents)}) {
        if (p.isEmpty())
            continue;
        bool covered = false;
        for (const QString& o : out)
            covered = covered || isInside(p, o);
        if (!covered)
            out << p;
    }
    return out;
}

} // namespace picazoo::platform
