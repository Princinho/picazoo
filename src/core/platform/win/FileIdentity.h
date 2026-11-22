#pragma once

// Identité d'un fichier (research R4) : identifiant NTFS + empreinte rapide.
// Ouverture avec FILE_READ_ATTRIBUTES seulement : aucun accès au contenu, aucun verrou.

#include <QByteArray>
#include <QString>

#include <optional>

namespace picazoo::platform {

struct FileId {
    quint64 volumeSerial = 0;
    QByteArray id; // 128 bits (FILE_ID_128) ; vide sur les systèmes sans identifiant stable
};

std::optional<FileId> fileIdentity(const QString& path);

// XXH3-64 de (taille + 64 Ko du début + 64 Ko de la fin). Vide si illisible.
std::optional<quint64> quickHash(const QString& path, qint64 size);

// XXH3-64 du fichier entier (tests et vérifications).
std::optional<quint64> fullHash(const QString& path);

// Numéro de série du volume qui contient `path`, ou vide.
std::optional<quint64> volumeSerial(const QString& path);

} // namespace picazoo::platform
