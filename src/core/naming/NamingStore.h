#pragma once

// Lectures et décisions sur les images liées pour l'interface (contracts/core-api.md).
// Connexions propres à library.db (lecture + écriture des décisions) ; ne lit ni ne modifie
// jamais un fichier de l'utilisateur (FR-017). Les décisions préviennent le moteur, qui
// recalcule les racines concernées.

#include "naming/NamingTypes.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

namespace picazoo {

class Database;

class NamingStore {
public:
    explicit NamingStore(const QString& libraryDbPath);
    ~NamingStore();

    // Appelé après chaque décision, avec les racines à recalculer.
    void setOnChanged(std::function<void(const QStringList& rootKeys)> fn) { onChanged_ = std::move(fn); }

    // --- Lectures
    QList<NamingGroupId> groups(ConfidenceFilter filter = ConfidenceFilter::All) const;
    QList<NamingGroupRow> rows(ConfidenceFilter filter = ConfidenceFilter::All) const; // triées par nom
    std::optional<NamingGroup> group(NamingGroupId id) const;                          // membres triés par nom
    std::optional<NamingGroupId> groupOf(ItemId item) const;
    // Groupe d'un élément sans ses membres ni signature (panneau d'informations).
    std::optional<NamingGroupRow> rowOf(ItemId item) const;
    NamingSummary summary() const;
    QList<NamingDecision> decisions() const; // la plus récente d'abord
    // Éléments de la bibliothèque dont le nom contient `text`, absents du groupe `exclude`.
    QList<NamingItem> searchItems(const QString& text, NamingGroupId exclude, int limit = 200) const;

    // --- Décisions (FR-008, FR-009). Toute correction confirme le groupe.
    bool addToGroup(NamingGroupId group, ItemId item);
    bool removeFromGroup(NamingGroupId group, ItemId item);
    // Renvoie le groupe conservé (`into`), 0 si la fusion est impossible.
    NamingGroupId mergeGroups(NamingGroupId into, NamingGroupId from);
    // Renvoie le nouveau groupe, 0 si l'une des deux parties aurait moins de deux éléments.
    NamingGroupId splitGroup(NamingGroupId group, const QList<ItemId>& toSplitOff, const QString& newName);
    bool rename(NamingGroupId group, const QString& displayName);
    bool confirm(NamingGroupId group);
    bool markNotAGroup(NamingGroupId group);
    bool undoDecision(const QString& rootKey);

private:
    void changed(const QStringList& keys);

    std::unique_ptr<Database> read_;
    std::unique_ptr<Database> write_;
    std::function<void(const QStringList&)> onChanged_;
};

} // namespace picazoo
