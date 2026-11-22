#pragma once

// Liste d'un dossier via FindFirstFileExW (FindExInfoBasic, FIND_FIRST_EX_LARGE_FETCH),
// research R3. Ignore fichiers cachés ou système, points de réanalyse (jonctions,
// liens symboliques), Thumbs.db et desktop.ini.

#include <QString>

#include <vector>

namespace picazoo::platform {

struct DirEntry {
    QString name;
    bool isDir = false;
    qint64 size = 0;
    qint64 mtime = 0; // epoch ms (UTC)
};

enum class ListResult { Ok, NotFound, Denied, Error };

ListResult listDirectory(const QString& dir, std::vector<DirEntry>& out);

// Vrai si le dossier existe et est accessible.
ListResult probeDirectory(const QString& dir);

// Existence et nature d'un chemin, sans l'ouvrir.
bool pathExists(const QString& path, bool* isDir = nullptr);

// FILETIME -> epoch ms.
qint64 fileTimeToEpochMs(quint32 low, quint32 high);

} // namespace picazoo::platform
