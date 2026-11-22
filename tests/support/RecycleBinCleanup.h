#pragma once

// Tests de la corbeille (002) : retire de la corbeille les seuls fichiers mis à la corbeille
// depuis un dossier de test, pour ne rien laisser derrière les tests.

#include "platform/win/Paths.h"
#include "platform/win/RecycleBin.h"

#include <windows.h>

#include <QString>

namespace picazoo::test {

inline int purgeRecycledUnder(const QString& folder)
{
    const QString root = platform::normalizePath(folder);
    int n = 0;
    for (const auto& e : platform::recycledEntries(root)) {
        if (!platform::isInside(e.originalPath, root))
            continue;
        DeleteFileW(platform::toWin32(e.recycledPath).c_str());
        DeleteFileW(platform::toWin32(e.infoPath).c_str());
        ++n;
    }
    return n;
}

} // namespace picazoo::test
