#pragma once

// Génère des fichiers de test : JPEG avec EXIF (orientation, date, appareil,
// vignette intégrée), PNG, JPEG tronqué, fichiers cachés, jonctions.

#include <QByteArray>
#include <QColor>
#include <QDateTime>
#include <QImage>
#include <QSize>
#include <QString>

#include <optional>

namespace picazoo::test {

struct JpegSpec {
    QSize size{64, 48};
    int orientation = 1;                 // EXIF 1–8
    std::optional<QDateTime> takenAt;    // DateTimeOriginal (heure de l'appareil)
    QString make;
    QString model;
    bool embeddedThumb = false;          // vignette IFD1
    QColor top = Qt::red;                // moitié haute des pixels stockés
    QColor bottom = Qt::blue;            // moitié basse
};

class FixtureFactory {
public:
    static QImage twoTone(QSize size, QColor top, QColor bottom);
    static QByteArray exifBlock(const JpegSpec& spec, const QByteArray& thumbJpeg = {});
    static QByteArray jpeg(const JpegSpec& spec = {});
    static QByteArray png(QSize size = {32, 32}, QColor color = Qt::green);

    // Doublons (002) : scène procédurale riche en détails, reproductible par sa graine.
    // `motion` déplace quelques formes (sujet qui bouge entre deux photos d'une rafale).
    static QImage scene(QSize size, quint32 seed, int motion = 0);
    // JPEG d'une image donnée, avec le bloc EXIF de `spec` (date, orientation…).
    static QByteArray jpegOf(const QImage& image, const JpegSpec& spec = {}, int quality = 90);
    static QString writeImage(const QString& path, const QImage& image, const JpegSpec& spec = {}, int quality = 90);
    static QString writeImagePng(const QString& path, const QImage& image);

    // Écrivent un fichier et renvoient son chemin.
    static QString writeJpeg(const QString& path, const JpegSpec& spec = {});
    static QString writePng(const QString& path, QSize size = {32, 32});
    static QString writeTruncatedJpeg(const QString& path);
    static QString writeBytes(const QString& path, const QByteArray& data);

    static bool setHidden(const QString& path);
    static bool makeJunction(const QString& link, const QString& target);
    static bool setMtime(const QString& path, const QDateTime& when);
};

} // namespace picazoo::test
