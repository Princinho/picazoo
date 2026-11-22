#pragma once

// Réglages de la visionneuse autonome (data-model « ViewerSettings »), dans
// viewer.ini du stockage de Picazoo. Seule écriture de la visionneuse sur le disque
// (contracts/storage-and-registry.md) ; une valeur n'est écrite que si elle change.

#include <QDateTime>
#include <QSettings>
#include <QStringList>

#include <memory>

class ViewerSettings {
public:
    // `dataDir` : PICAZOO_DATA_DIR ou %LOCALAPPDATA%\Picazoo.
    explicit ViewerSettings(const QString& dataDir);

    static QString defaultDataDir();

    bool opaque() const;
    void setOpaque(bool v);
    bool stripVisible() const;
    void setStripVisible(bool v);

    // none | later | never | accepted
    QString assocAnswer() const;
    QDateTime assocAskedAt() const;
    void setAssocAnswer(const QString& answer, const QStringList& formats = {});
    QStringList assocFormats() const;

    QString filePath() const;

private:
    void set(const QString& key, const QVariant& value);
    QString dir_;
    std::unique_ptr<QSettings> s_;
};
