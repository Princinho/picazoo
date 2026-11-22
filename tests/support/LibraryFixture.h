#pragma once

// Bibliothèque de test : données Picazoo dans un dossier temporaire distinct des photos.

#include "library/Library.h"
#include "support/TempTree.h"

#include <QtTest>

#include <memory>

namespace picazoo::test {

class LibraryFixture {
public:
    LibraryFixture() { open(); }

    void open() { lib = Library::open(data.path()); }
    void close() { lib.reset(); }
    void reopen()
    {
        close();
        open();
    }

    bool idle(int timeoutMs = 30000, bool thumbs = true) { return lib->waitForIdle(timeoutMs, thumbs); }

    // Somme des éléments par nom de dossier.
    QMap<QString, int> countsByFolder() const
    {
        QMap<QString, int> out;
        for (const auto& f : lib->folders(FolderSort::NameAsc))
            out[f.name] += f.itemCount;
        return out;
    }

    TempTree data;
    std::unique_ptr<Library> lib;
};

} // namespace picazoo::test
