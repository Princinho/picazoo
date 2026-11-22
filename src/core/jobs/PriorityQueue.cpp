#include "jobs/PriorityQueue.h"

#include <unordered_set>

namespace picazoo {

void PriorityQueue::enqueueLocked(ItemId id, Priority p)
{
    member_[id] = p;
    if (p == Priority::Background)
        levels_[int(p)].push_back(id);
    else
        levels_[int(p)].push_front(id);
}

void PriorityQueue::push(ItemId id, Priority p)
{
    {
        std::lock_guard lock(mutex_);
        const auto it = member_.find(id);
        if (it != member_.end() && int(it->second) <= int(p))
            return;
        enqueueLocked(id, p); // l'ancienne entrée devient périmée (suppression paresseuse)
    }
    cv_.notify_one();
}

void PriorityQueue::reprioritize(const QList<ItemId>& visible, const QList<ItemId>& nearby)
{
    {
        std::lock_guard lock(mutex_);
        std::unordered_set<ItemId> keep(visible.begin(), visible.end());
        keep.insert(nearby.begin(), nearby.end());
        // Rétrograder ce qui n'est plus visible ni proche.
        for (int level : {0, 1}) {
            for (ItemId id : levels_[level]) {
                auto it = member_.find(id);
                if (it != member_.end() && int(it->second) == level && !keep.contains(id))
                    enqueueLocked(id, Priority::Background);
            }
            levels_[level].clear();
        }
        // Rebâtir les niveaux 0 et 1 dans l'ordre donné, seulement pour les éléments en attente.
        for (auto it = nearby.rbegin(); it != nearby.rend(); ++it) {
            if (member_.contains(*it))
                enqueueLocked(*it, Priority::Nearby);
        }
        for (auto it = visible.rbegin(); it != visible.rend(); ++it) {
            if (member_.contains(*it))
                enqueueLocked(*it, Priority::Visible);
        }
    }
    cv_.notify_all();
}

std::optional<QueuedItem> PriorityQueue::takeLocked()
{
    for (int index = 0; index < 3; ++index) {
        auto& level = levels_[index];
        while (!level.empty()) {
            const ItemId id = level.front();
            level.pop_front();
            auto it = member_.find(id);
            if (it != member_.end() && int(it->second) == index) {
                member_.erase(it);
                ++active_;
                return QueuedItem{id, Priority(index)};
            }
        }
    }
    return std::nullopt;
}

std::optional<QueuedItem> PriorityQueue::pop()
{
    std::unique_lock lock(mutex_);
    for (;;) {
        if (stopped_)
            return std::nullopt;
        if (!paused_) {
            if (auto id = takeLocked())
                return id;
        }
        cv_.wait(lock);
    }
}

std::optional<QueuedItem> PriorityQueue::tryPop()
{
    std::lock_guard lock(mutex_);
    if (stopped_ || paused_)
        return std::nullopt;
    return takeLocked();
}

bool PriorityQueue::remove(ItemId id)
{
    std::lock_guard lock(mutex_);
    return member_.erase(id) > 0;
}

void PriorityQueue::pause()
{
    std::lock_guard lock(mutex_);
    paused_ = true;
}

void PriorityQueue::resume()
{
    {
        std::lock_guard lock(mutex_);
        paused_ = false;
    }
    cv_.notify_all();
}

bool PriorityQueue::paused() const
{
    std::lock_guard lock(mutex_);
    return paused_;
}

void PriorityQueue::stop()
{
    {
        std::lock_guard lock(mutex_);
        stopped_ = true;
    }
    cv_.notify_all();
}

size_t PriorityQueue::size() const
{
    std::lock_guard lock(mutex_);
    return member_.size();
}

void PriorityQueue::taskDone()
{
    std::lock_guard lock(mutex_);
    if (active_ > 0)
        --active_;
}

size_t PriorityQueue::outstanding() const
{
    std::lock_guard lock(mutex_);
    return member_.size() + active_;
}

bool PriorityQueue::contains(ItemId id) const
{
    std::lock_guard lock(mutex_);
    return member_.contains(id);
}

} // namespace picazoo
