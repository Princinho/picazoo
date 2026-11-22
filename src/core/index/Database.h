#pragma once

// Enveloppe minimale de l'API C de SQLite (research R10).

#include <QByteArray>
#include <QString>

#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>

struct sqlite3;
struct sqlite3_stmt;

namespace picazoo {

class DbError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Statement {
public:
    Statement() = default;
    Statement(sqlite3* db, const char* sql);
    ~Statement();
    Statement(Statement&&) noexcept;
    Statement& operator=(Statement&&) noexcept;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement& bind(int index, qint64 value);
    Statement& bind(int index, int value) { return bind(index, qint64(value)); }
    Statement& bind(int index, const QString& value);
    Statement& bind(int index, const QByteArray& value); // BLOB
    Statement& bindNull(int index);
    template <typename T> Statement& bind(int index, const std::optional<T>& value)
    {
        return value ? bind(index, *value) : bindNull(index);
    }

    // Avance d'une ligne. Vrai si une ligne est disponible.
    bool step();
    // Exécute jusqu'au bout (INSERT/UPDATE/DELETE).
    void run();
    void reset();

    bool isNull(int col) const;
    qint64 int64(int col) const;
    int int32(int col) const { return int(int64(col)); }
    QString text(int col) const;
    QByteArray blob(int col) const;
    std::optional<qint64> optInt64(int col) const;

private:
    sqlite3* db_ = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
};

class Database {
public:
    // Ouvre ou crée la base ; applique WAL, synchronous=NORMAL, foreign_keys, busy_timeout.
    static std::unique_ptr<Database> open(const QString& path, bool readOnly = false);
    ~Database();

    void exec(const char* sql);
    void exec(const QString& sql) { exec(sql.toUtf8().constData()); }
    Statement prepare(const char* sql) { return Statement(db_, sql); }
    qint64 lastInsertId() const;
    int changes() const;
    sqlite3* handle() const { return db_; }

    // Les connexions SQLite sont sérialisées par ce verrou quand plusieurs threads les partagent.
    std::recursive_mutex& mutex() { return mutex_; }

private:
    explicit Database(sqlite3* db) : db_(db) {}
    sqlite3* db_ = nullptr;
    std::recursive_mutex mutex_;
};

class Transaction {
public:
    explicit Transaction(Database& db);
    ~Transaction();
    void commit();

private:
    Database& db_;
    bool done_ = false;
};

} // namespace picazoo
