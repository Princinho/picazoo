#include "index/ThumbStore.h"

#include "index/Database.h"

namespace picazoo {

ThumbStore::ThumbStore(const QString& path, qint64 cacheBytesPerConnection)
{
    write_ = Database::open(path, false, cacheBytesPerConnection);
    write_->exec("CREATE TABLE IF NOT EXISTS thumb ("
                 " item_id INTEGER NOT NULL,"
                 " kind    TEXT    NOT NULL CHECK (kind IN ('provisional', 'final')),"
                 " version INTEGER NOT NULL,"
                 " width   INTEGER NOT NULL,"
                 " height  INTEGER NOT NULL,"
                 " jpeg    BLOB    NOT NULL,"
                 " PRIMARY KEY (item_id, kind)) WITHOUT ROWID");
    read_ = Database::open(path, true, cacheBytesPerConnection);
}

ThumbStore::~ThumbStore() = default;

std::optional<StoredThumb> ThumbStore::get(ItemId id, ThumbKind kind, int version) const
{
    std::lock_guard lock(read_->mutex());
    auto q = read_->prepare("SELECT version, width, height, jpeg FROM thumb WHERE item_id=? AND kind=?");
    q.bind(1, id).bind(2, toDb(kind));
    if (!q.step() || q.int32(0) != version)
        return std::nullopt;
    StoredThumb t;
    t.itemId = id;
    t.kind = kind;
    t.version = version;
    t.size = QSize(q.int32(1), q.int32(2));
    t.jpeg = q.blob(3);
    return t;
}

void ThumbStore::put(const StoredThumb& thumb) { putBatch({thumb}); }

void ThumbStore::putBatch(const QList<StoredThumb>& thumbs)
{
    if (thumbs.isEmpty())
        return;
    Transaction tx(*write_);
    auto q = write_->prepare("INSERT OR REPLACE INTO thumb (item_id, kind, version, width, height, jpeg)"
                             " VALUES (?, ?, ?, ?, ?, ?)");
    for (const auto& t : thumbs) {
        q.bind(1, t.itemId).bind(2, toDb(t.kind)).bind(3, t.version);
        q.bind(4, t.size.width()).bind(5, t.size.height()).bind(6, t.jpeg);
        q.run();
    }
    tx.commit();
}

void ThumbStore::remove(const QList<ItemId>& ids)
{
    if (ids.isEmpty())
        return;
    Transaction tx(*write_);
    auto q = write_->prepare("DELETE FROM thumb WHERE item_id=?");
    for (ItemId id : ids) {
        q.bind(1, id);
        q.run();
    }
    tx.commit();
}

qint64 ThumbStore::count() const
{
    std::lock_guard lock(read_->mutex());
    auto q = read_->prepare("SELECT COUNT(*) FROM thumb");
    return q.step() ? q.int64(0) : 0;
}

} // namespace picazoo
