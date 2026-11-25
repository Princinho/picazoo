#pragma once

// Corbeille de Windows (research D8, D9). C'est le SEUL code de Picazoo qui supprime un
// fichier de l'utilisateur, et il ne fait que l'envoyer à la corbeille : tout élément que
// le système effacerait définitivement (disque sans corbeille, fichier trop gros, corbeille
// désactivée…) est refusé et laissé en place.
//
// Les fonctions initialisent COM (STA) sur le fil appelant si besoin ; à appeler hors du fil
// de l'interface.

#include <QList>
#include <QString>

namespace picazoo::platform {

// Vrai si le volume de `path` est un disque fixe doté d'une corbeille active.
bool volumeHasRecycleBin(const QString& path);

enum class RecycleResult { Recycled, NoRecycleBin, Locked, Missing, Failed };

// 007 : même réponse, mise en cache par racine de volume (menus contextuels). Hors du fil de
// l'interface au premier appel pour un volume.
bool volumeHasRecycleBinCached(const QString& path);

// Met `path` à la corbeille, jamais d'effacement définitif.
RecycleResult moveToRecycleBin(const QString& path, QString* message = nullptr);

// Fichier de la corbeille de l'utilisateur courant (lu dans les fichiers $I, sans rien modifier).
struct RecycledEntry {
    QString originalPath;
    QString recycledPath; // fichier $R… dans $Recycle.Bin
    QString infoPath;     // fichier $I… correspondant
    qint64 deletedAtMs = 0;
    qint64 sizeBytes = 0;
};
QList<RecycledEntry> recycledEntries(const QString& anyPathOnVolume);

enum class RestoreResult { Restored, NotInRecycleBin, Conflict, Failed };

// Restaure depuis la corbeille le fichier `originalPath` supprimé après `deletedAfterMs`
// (le plus récent s'il y en a plusieurs). Refuse si un fichier occupe déjà l'emplacement.
RestoreResult restoreFromRecycleBin(const QString& originalPath, qint64 deletedAfterMs, QString* message = nullptr);

} // namespace picazoo::platform
