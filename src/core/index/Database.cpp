#include "index/Database.h"

#include <sqlite3.h>

#include <utility>

namespace picazoo {

namespace {

[[noreturn]] void fail(sqlite3* db, const char* what)
{
    throw DbError(std::string(what) + " : " + (db ? sqlite3_errmsg(db) : "base absente"));
}

} // namespace

// ---------------------------------------------------------------- Statement

Statement::Statement(sqlite3* db, const char* sql) : db_(db)
{
    if (sqlite3_prepare_v3(db, sql, -1, SQLITE_PREPARE_PERSISTENT, &stmt_, nullptr) != SQLITE_OK)
        fail(db, sql);
}

Statement::~Statement()
{
    if (stmt_)
        sqlite3_finalize(stmt_);
}

Statement::Statement(Statement&& o) noexcept : db_(o.db_), stmt_(std::exchange(o.stmt_, nullptr)) {}

Statement& Statement::operator=(Statement&& o) noexcept
{
    if (this != &o) {
        if (stmt_)
            sqlite3_finalize(stmt_);
        db_ = o.db_;
        stmt_ = std::exchange(o.stmt_, nullptr);
    }
    return *this;
}

Statement& Statement::bind(int index, qint64 value)
{
    if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK)
        fail(db_, "bind int");
    return *this;
}

Statement& Statement::bind(int index, const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    if (sqlite3_bind_text(stmt_, index, utf8.constData(), int(utf8.size()), SQLITE_TRANSIENT) != SQLITE_OK)
        fail(db_, "bind text");
    return *this;
}

Statement& Statement::bind(int index, const QByteArray& value)
{
    if (sqlite3_bind_blob(stmt_, index, value.constData(), int(value.size()), SQLITE_TRANSIENT) != SQLITE_OK)
        fail(db_, "bind blob");
    return *this;
}

Statement& Statement::bindNull(int index)
{
    if (sqlite3_bind_null(stmt_, index) != SQLITE_OK)
        fail(db_, "bind null");
    return *this;
}

bool Statement::step()
{
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW)
        return true;
    if (rc == SQLITE_DONE)
        return false;
    fail(db_, "step");
}

void Statement::run()
{
    while (step()) {
    }
    reset();
}

void Statement::reset()
{
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

bool Statement::isNull(int col) const { return sqlite3_column_type(stmt_, col) == SQLITE_NULL; }

qint64 Statement::int64(int col) const { return sqlite3_column_int64(stmt_, col); }

QString Statement::text(int col) const
{
    const auto* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, col));
    const int n = sqlite3_column_bytes(stmt_, col);
    return p ? QString::fromUtf8(p, n) : QString();
}

QByteArray Statement::blob(int col) const
{
    const void* p = sqlite3_column_blob(stmt_, col);
    const int n = sqlite3_column_bytes(stmt_, col);
    return p ? QByteArray(static_cast<const char*>(p), n) : QByteArray();
}

std::optional<qint64> Statement::optInt64(int col) const
{
    if (isNull(col))
        return std::nullopt;
    return int64(col);
}

// ----------------------------------------------------------------- Database

std::unique_ptr<Database> Database::open(const QString& path, bool readOnly)
{
    sqlite3* db = nullptr;
    const int flags = (readOnly ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)) |
                      SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(path.toUtf8().constData(), &db, flags, nullptr) != SQLITE_OK) {
        const std::string msg = db ? sqlite3_errmsg(db) : "ouverture impossible";
        sqlite3_close(db);
        throw DbError("Ouverture de " + path.toStdString() + " : " + msg);
    }
    std::unique_ptr<Database> out(new Database(db));
    sqlite3_busy_timeout(db, 5000);
    if (!readOnly) {
        out->exec("PRAGMA journal_mode=WAL");
        out->exec("PRAGMA synchronous=NORMAL");
    }
    out->exec("PRAGMA foreign_keys=ON");
    out->exec("PRAGMA temp_store=MEMORY");
    out->exec("PRAGMA cache_size=-32768"); // 32 Mo
    return out;
}

Database::~Database()
{
    if (db_)
        sqlite3_close_v2(db_);
}

void Database::exec(const char* sql)
{
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        const std::string msg = err ? err : "erreur";
        sqlite3_free(err);
        throw DbError(msg + " [" + sql + "]");
    }
}

qint64 Database::lastInsertId() const { return sqlite3_last_insert_rowid(db_); }

int Database::changes() const { return sqlite3_changes(db_); }

// -------------------------------------------------------------- Transaction

Transaction::Transaction(Database& db) : db_(db)
{
    db_.mutex().lock();
    try {
        db_.exec("BEGIN IMMEDIATE");
    } catch (...) {
        db_.mutex().unlock();
        throw;
    }
}

Transaction::~Transaction()
{
    if (!done_) {
        try {
            db_.exec("ROLLBACK");
        } catch (...) {
        }
    }
    db_.mutex().unlock();
}

void Transaction::commit()
{
    db_.exec("COMMIT");
    done_ = true;
}

} // namespace picazoo
