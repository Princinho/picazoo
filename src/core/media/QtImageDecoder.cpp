#include "media/QtImageDecoder.h"

#include <QBuffer>
#include <QImageReader>

namespace picazoo {

std::optional<QImage> QtImageDecoder::decode(const QByteArray& data, QSize target, QSize* fullSize)
{
    QByteArray copy = data; // QBuffer exige un tableau modifiable ; copie paresseuse (COW)
    QBuffer buffer(&copy);
    if (!buffer.open(QIODevice::ReadOnly))
        return std::nullopt;
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    reader.setAllocationLimit(1024); // Mo
    const QSize full = reader.size();
    if (fullSize)
        *fullSize = full;
    if (!target.isEmpty() && full.isValid() && (full.width() > target.width() || full.height() > target.height()))
        reader.setScaledSize(full.scaled(target, Qt::KeepAspectRatio));
    QImage img = reader.read();
    if (img.isNull())
        return std::nullopt;
    if (fullSize && !full.isValid())
        *fullSize = img.size();
    return img;
}

QSize QtImageDecoder::readSize(const QByteArray& data)
{
    QByteArray copy = data;
    QBuffer buffer(&copy);
    if (!buffer.open(QIODevice::ReadOnly))
        return {};
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    QSize s = reader.size();
    if (reader.transformation() & QImageIOHandler::TransformationRotate90)
        s.transpose();
    return s;
}

} // namespace picazoo
