#pragma once

// Métadonnées d'une image (research R7) : date de prise de vue, orientation,
// appareil, dimensions, vignette intégrée.

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>

#include <optional>
#include <string>

namespace picazoo {

struct Metadata {
    bool ok = false;
    std::optional<qint64> takenAt; // epoch ms, heure de l'appareil sans fuseau (stockée comme UTC)
    int orientation = 1;           // EXIF 1–8
    QString camera;                // « Marque Modèle »
    QSize size;                    // dimensions stockées dans le fichier (avant orientation)
    std::optional<qint64> durationMs;
    QByteArray embeddedThumb;      // JPEG de la vignette intégrée, si présente
};

// Analyse le début d'un fichier JPEG (segments APP1 Exif et SOFn).
Metadata parseJpegMetadata(const QByteArray& filePrefix);

// Analyse un bloc EXIF commençant par "Exif\0\0" ou directement par l'en-tête TIFF.
Metadata parseExifBlock(const QByteArray& block);

// "YYYY:MM:DD HH:MM:SS" -> epoch ms (UTC naïf). Vide si invalide.
std::optional<qint64> parseExifDate(const std::string& s);

// Dimensions après application de l'orientation (5–8 échangent largeur et hauteur).
QSize orientedSize(QSize size, int orientation);

// Applique l'orientation EXIF (8 cas) à une image décodée.
QImage applyOrientation(QImage image, int orientation);

// Lit les métadonnées d'un fichier JPEG sur disque (au plus 256 Ko lus).
Metadata readJpegMetadata(const QString& path);

} // namespace picazoo
