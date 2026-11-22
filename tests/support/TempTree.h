#pragma once

// Dossier de test temporaire, supprimé à la fin du test.

#include <QByteArray>
#include <QDir>
#include <QString>
#include <QTemporaryDir>

namespace picazoo::test {

class TempTree {
public:
    TempTree() : dir_(QDir::tempPath() + QStringLiteral("/picazoo-test-XXXXXX")) {}

    bool isValid() const { return dir_.isValid(); }
    // Chemin natif normalisé (séparateurs '\').
    QString path() const { return QDir::toNativeSeparators(QDir(dir_.path()).canonicalPath()); }
    QString file(const QString& rel) const { return QDir::toNativeSeparators(path() + u'/' + rel); }

    QString mkdir(const QString& rel) const
    {
        QDir().mkpath(file(rel));
        return file(rel);
    }

    QString write(const QString& rel, const QByteArray& data) const;

private:
    QTemporaryDir dir_;
};

} // namespace picazoo::test
