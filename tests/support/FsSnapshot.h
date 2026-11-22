#pragma once

// Instantané d'un dossier (chemins, tailles, dates, hachages) pour vérifier
// l'invariant SC-006 : Picazoo ne crée, ne modifie, ne renomme ni ne supprime rien.

#include <QMap>
#include <QString>
#include <QStringList>

namespace picazoo::test {

class FsSnapshot {
public:
    struct Entry {
        bool isDir = false;
        qint64 size = 0;
        qint64 mtime = 0;
        quint64 hash = 0;
        bool operator==(const Entry&) const = default;
    };

    static FsSnapshot take(const QString& root);

    // Liste lisible des différences ; vide si identiques.
    QStringList diff(const FsSnapshot& after) const;
    qsizetype size() const { return entries_.size(); }

private:
    QMap<QString, Entry> entries_;
};

} // namespace picazoo::test

// Vérifie que le dossier n'a pas changé depuis l'instantané `before`.
#define PICAZOO_VERIFY_UNTOUCHED(before, root)                                                                    \
    do {                                                                                                          \
        const QStringList _d = (before).diff(picazoo::test::FsSnapshot::take(root));                             \
        if (!_d.isEmpty())                                                                                        \
            QFAIL(qPrintable(QStringLiteral("Fichiers modifiés par Picazoo :\n") + _d.join(u'\n')));            \
    } while (0)
