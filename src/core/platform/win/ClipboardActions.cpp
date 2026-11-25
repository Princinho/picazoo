#include "platform/win/ClipboardActions.h"

#include <windows.h>

#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QMimeData>
#include <QUrl>

namespace picazoo::platform {

QString pathsAsText(const QStringList& paths)
{
    QStringList lines;
    for (const QString& p : paths)
        lines << QDir::toNativeSeparators(p);
    return lines.join(QStringLiteral("\r\n"));
}

bool copyFilesToClipboard(const QStringList& paths, const QImage& image)
{
    auto* clipboard = QGuiApplication::clipboard();
    if (!clipboard || paths.isEmpty())
        return false;
    auto* mime = new QMimeData;
    QList<QUrl> urls;
    for (const QString& p : paths)
        urls << QUrl::fromLocalFile(QDir::fromNativeSeparators(p)); // converti en CF_HDROP par Qt
    mime->setUrls(urls);
    // L'Explorateur lit cet effet au moment de coller : DROPEFFECT_COPY (1), jamais un déplacement.
    const DWORD effect = 1; // DROPEFFECT_COPY
    mime->setData(QStringLiteral("application/x-qt-windows-mime;value=\"Preferred DropEffect\""),
                  QByteArray(reinterpret_cast<const char*>(&effect), sizeof(effect)));
    if (!image.isNull())
        mime->setImageData(image);
    clipboard->setMimeData(mime);
    return true;
}

bool copyTextToClipboard(const QString& text)
{
    auto* clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return false;
    clipboard->setText(text);
    return true;
}

} // namespace picazoo::platform
