#include "ViewerSettings.h"

#include <QDir>
#include <QStandardPaths>

ViewerSettings::ViewerSettings(const QString& dataDir) : dir_(dataDir)
{
    // QSettings ne crée pas le fichier tant que rien n'est écrit.
    s_ = std::make_unique<QSettings>(filePath(), QSettings::IniFormat);
}

QString ViewerSettings::defaultDataDir()
{
    if (qEnvironmentVariableIsSet("PICAZOO_DATA_DIR"))
        return qEnvironmentVariable("PICAZOO_DATA_DIR");
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Picazoo");
}

QString ViewerSettings::filePath() const { return QDir(dir_).filePath(QStringLiteral("viewer.ini")); }

void ViewerSettings::set(const QString& key, const QVariant& value)
{
    if (s_->contains(key) && s_->value(key) == value)
        return;
    QDir().mkpath(dir_);
    s_->setValue(key, value);
    s_->sync();
}

bool ViewerSettings::opaque() const { return s_->value(QStringLiteral("overlay/opaque"), false).toBool(); }
void ViewerSettings::setOpaque(bool v) { set(QStringLiteral("overlay/opaque"), v); }

bool ViewerSettings::stripVisible() const { return s_->value(QStringLiteral("strip/visible"), true).toBool(); }
void ViewerSettings::setStripVisible(bool v) { set(QStringLiteral("strip/visible"), v); }

QString ViewerSettings::assocAnswer() const
{
    return s_->value(QStringLiteral("assoc/answer"), QStringLiteral("none")).toString();
}

QDateTime ViewerSettings::assocAskedAt() const
{
    return QDateTime::fromString(s_->value(QStringLiteral("assoc/askedAt")).toString(), Qt::ISODate);
}

QStringList ViewerSettings::assocFormats() const { return s_->value(QStringLiteral("assoc/formats")).toStringList(); }

void ViewerSettings::setAssocAnswer(const QString& answer, const QStringList& formats)
{
    set(QStringLiteral("assoc/answer"), answer);
    set(QStringLiteral("assoc/askedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!formats.isEmpty())
        set(QStringLiteral("assoc/formats"), formats);
}
