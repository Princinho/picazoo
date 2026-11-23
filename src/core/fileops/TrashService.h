#pragma once

// Mise à la corbeille des copies écartées, journal et annulation (FR-016 à FR-023,
// research D8, D9, contracts/core-api.md). Garanties :
//  - rien sans un plan produit par plan() et confirmé ;
//  - au moins une copie de chaque groupe reste en place ;
//  - juste avant d'agir, chaque fichier et la copie gardée sont revérifiés ;
//  - jamais d'effacement définitif (platform::moveToRecycleBin) ;
//  - le journal est écrit avant le premier fichier traité.

#include "duplicates/DuplicateTypes.h"

#include <QList>
#include <QObject>
#include <QString>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace picazoo {

class Database;
class DuplicateEngine;
class Library;

enum class BlockReason { NoRecycleBin, Unavailable, Locked };

struct TrashPlanEntry {
    ItemId item = 0;
    GroupId group = 0;
    QString path;
    qint64 bytes = 0;
    ItemId keeper = 0;
    QString keeperPath;
    bool identical = false; // contenu identique à la copie gardée (vérifié par empreinte)
};

struct BlockedEntry {
    ItemId item = 0;
    GroupId group = 0;
    QString path;
    BlockReason reason = BlockReason::NoRecycleBin;
};

struct TrashPlan {
    QList<TrashPlanEntry> toTrash;
    QList<BlockedEntry> blocked;
    QList<GroupId> refused; // groupes dont aucune copie ne resterait (FR-017)
    qint64 bytes = 0;
};

enum class OperationState { Planned, Running, Done, Partial, Failed, Undone, PartiallyUndone };

struct OperationEntry {
    qint64 id = 0;
    std::optional<ItemId> item;
    QString path;
    qint64 sizeBytes = 0;
    QString keeperPath;
    QString result; // valeurs de file_operation_entry.result
    QString message;
};

struct Operation {
    OperationId id = 0;
    QString kind; // trash | restore
    std::optional<OperationId> undoes;
    qint64 createdAt = 0;
    std::optional<qint64> finishedAt;
    OperationState state = OperationState::Planned;
    int fileCount = 0;
    qint64 bytes = 0;
    QList<OperationEntry> entries;
};

QString toDb(OperationState);
OperationState operationStateFromDb(const QString&);
QString resultLabel(const QString& result); // texte affiché (français)

class TrashService : public QObject {
    Q_OBJECT

public:
    TrashService(Library& library, DuplicateEngine& engine, QObject* parent = nullptr);
    ~TrashService() override;

    // Récapitulatif ; ne touche à rien (FR-016).
    TrashPlan plan(const QList<GroupId>& groups) const;
    TrashPlan planAllIdentical() const; // FR-022

    // Exécute un plan confirmé, en arrière-plan. 0 si refusé (opération déjà en cours, plan vide).
    OperationId execute(const TrashPlan& confirmed);
    // Restaure depuis la corbeille ce qui y est encore (FR-021). 0 si impossible.
    OperationId undo(OperationId id);

    QList<Operation> journal() const; // le plus récent en premier, sans le détail
    std::optional<Operation> operation(OperationId id) const;
    bool busy() const { return busy_.load(); }
    bool waitForIdle(int timeoutMs); // tests

signals:
    void operationProgress(qint64 id, int done, int total);
    void operationFinished(qint64 id, int state);

private:
    void recoverInterrupted();
    void runTrash(OperationId id, TrashPlan plan);
    void runRestore(OperationId id, OperationId original, qint64 since);
    void setEntry(qint64 entryId, const QString& result, const QString& message);
    void finish(OperationId id, OperationState state);
    void join();

    Library& lib_;
    DuplicateEngine& engine_;
    std::unique_ptr<Database> db_;
    mutable std::recursive_mutex dbMutex_;
    std::thread worker_;
    std::atomic<bool> busy_{false};
};

} // namespace picazoo
