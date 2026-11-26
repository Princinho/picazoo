#pragma once

// Cache des vignettes dans thumbs.db (research R6, data-model.md « thumbs.db »).
// Peut être supprimé à tout moment : il se reconstruit à partir des fichiers.

#include "library/Types.h"

#include <QByteArray>
#include <QList>
#include <QSize>
#include <QString>

#include <memory>
#include <optional>

namespace picazoo {

class Database;

struct StoredThumb {
    ItemId itemId = 0;
    ThumbKind kind = ThumbKind::Final;
    int version = 1;
    QSize size;
    QByteArray jpeg;
};

class ThumbStore {
public:
    // cacheBytesPerConnection <= 0 : cache_size par défaut (voir Database::open).
    explicit ThumbStore(const QString& path, qint64 cacheBytesPerConnection = 0);
    ~ThumbStore();

    // Vide si absente ou si la version ne correspond plus.
    std::optional<StoredThumb> get(ItemId id, ThumbKind kind, int version) const;
    void put(const StoredThumb& thumb);
    void putBatch(const QList<StoredThumb>& thumbs);
    void remove(const QList<ItemId>& ids);
    qint64 count() const;

private:
    std::unique_ptr<Database> write_;
    std::unique_ptr<Database> read_;
};

} // namespace picazoo
