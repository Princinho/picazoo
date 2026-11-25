#pragma once

// Mise à la corbeille d'une liste de chemins, avec le journal des opérations (007, research T2).
// Utilisé par la visionneuse autonome (sans bibliothèque) ; TrashService s'en sert pour le
// passage à la corbeille d'un fichier (recycle()) et le libellé des origines.
//
// Garanties (constitution, principe I) :
//  - jamais d'effacement définitif (platform::moveToRecycleBin) ;
//  - l'opération et ses entrées « pending » sont écrites avant le premier fichier traité ;
//  - un fichier modifié depuis sa préparation (taille, date) est laissé en place.
//
// Appels bloquants : à faire hors du fil de l'interface.

#include "duplicates/DuplicateTypes.h"
#include "library/Types.h"

#include <QList>
#include <QString>

#include <optional>

namespace picazoo {

class Database;

enum class OperationSource { Duplicates, Library, Viewer };

QString toDb(OperationSource);
OperationSource operationSourceFromDb(const QString&);

struct LooseTrashEntry {
    std::optional<ItemId> item; // absent pour un fichier hors de la bibliothèque
    QString path;
    qint64 bytes = 0;
    qint64 mtime = 0; // ms depuis l'epoch, date de dernière écriture
};

class FileTrash {
public:
    explicit FileTrash(Database& db);

    // Décrit le fichier tel qu'il est maintenant (taille, date) ; rien si absent.
    static std::optional<LooseTrashEntry> describe(const QString& path, std::optional<ItemId> item = {});

    // Envoie le fichier à la corbeille et renvoie le résultat au format du journal
    // (« trashed », « skipped_no_recycle_bin », « skipped_locked », « skipped_changed », « failed »).
    static QString recycle(const QString& path, QString* message = nullptr);

    // Journal puis corbeille. Renvoie l'opération (0 si la liste est vide).
    OperationId trash(const QList<LooseTrashEntry>& entries, OperationSource source);

    // Restaure ce qui est encore dans la corbeille ; renvoie l'opération « restore » (0 si rien
    // à restaurer). Le nombre de fichiers restaurés est écrit dans `restored`.
    OperationId restore(OperationId operation, int* restored = nullptr);

private:
    void setEntry(qint64 entryId, const QString& result, const QString& message);
    Database& db_;
};

} // namespace picazoo
