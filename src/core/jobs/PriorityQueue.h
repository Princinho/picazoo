#pragma once

// File à trois niveaux de priorité (research R6, R12) :
// éléments visibles, éléments proches (±2 écrans), reste de la bibliothèque.
// Un élément n'est présent qu'une fois ; le reclasser le déplace.

#include "library/Types.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace picazoo {

enum class Priority { Visible = 0, Nearby = 1, Background = 2 };

struct QueuedItem {
    ItemId id = 0;
    Priority priority = Priority::Background;
};

class PriorityQueue {
public:
    // Ajoute ou relève la priorité (ne l'abaisse jamais).
    void push(ItemId id, Priority p = Priority::Background);
    // Les `visible` passent en tête, les `nearby` ensuite ; les éléments visibles ou proches
    // qui ne figurent plus dans les listes sont rétrogradés (pas annulés).
    void reprioritize(const QList<ItemId>& visible, const QList<ItemId>& nearby);
    // Attend un élément ; vide si la file est arrêtée.
    std::optional<QueuedItem> pop();
    // Non bloquant.
    std::optional<QueuedItem> tryPop();
    bool remove(ItemId id);

    // À appeler quand un élément obtenu par pop() a été entièrement traité.
    void taskDone();

    void pause();
    void resume();
    bool paused() const;
    void stop();
    size_t size() const;
    // En attente + en cours de traitement.
    size_t outstanding() const;
    bool contains(ItemId id) const;

private:
    std::optional<QueuedItem> takeLocked();
    void enqueueLocked(ItemId id, Priority p);

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<ItemId> levels_[3];
    std::unordered_map<ItemId, Priority> member_;
    bool paused_ = false;
    bool stopped_ = false;
    size_t active_ = 0;
};

} // namespace picazoo
