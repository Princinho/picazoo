#pragma once

// Préférences d'affichage, table `settings` de library.db (data-model.md).

#include <QHash>
#include <QString>
#include <QVariant>

#include <mutex>

namespace picazoo {

class Database;

class Settings {
public:
    explicit Settings(Database& db);

    // Valeur enregistrée, ou valeur par défaut documentée.
    QVariant value(const QString& key) const;
    // Les valeurs hors domaine sont ramenées à une valeur valide (ex. thumb_size borné à 64–320).
    void setValue(const QString& key, const QVariant& value);

    static QVariant defaultValue(const QString& key);
    static QVariant sanitize(const QString& key, const QVariant& value);

private:
    Database& db_;
    mutable std::mutex mutex_;
    QHash<QString, QString> cache_;
};

} // namespace picazoo
