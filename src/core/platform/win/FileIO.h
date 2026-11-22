#pragma once

// Lecture seule des fichiers de l'utilisateur (principe I de la constitution).
// Toutes les ouvertures se font en lecture, avec partage total
// (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE), sans verrou.
// Aucune fonction d'écriture n'existe dans ce module.

#include <QByteArray>
#include <QString>

namespace picazoo::platform {

// Lit au plus `maxBytes` octets à partir de `offset`. Renvoie un tableau vide
// en cas d'échec (`ok` vaut alors false).
QByteArray readFileRange(const QString& path, qint64 offset, qint64 maxBytes, bool* ok = nullptr);

// Lit tout le fichier (plafonné à `maxBytes`, 2 Go par défaut).
QByteArray readWholeFile(const QString& path, bool* ok = nullptr, qint64 maxBytes = qint64(2) << 30);

// Taille du fichier, ou -1.
qint64 fileSize(const QString& path);

} // namespace picazoo::platform
