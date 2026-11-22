#include "platform/win/Paths.h"

#include <QDir>

namespace picazoo::platform {

QString normalizePath(const QString& path)
{
    if (path.isEmpty())
        return {};
    QString s = QDir::fromNativeSeparators(path);
    if (s.startsWith(u"//?/"))
        s = s.mid(4);
    if (QDir::isRelativePath(s))
        s = QDir::current().absoluteFilePath(s);
    s = QDir::cleanPath(s);
    s = QDir::toNativeSeparators(s);
    if (s.size() >= 2 && s[1] == u':')
        s[0] = s[0].toUpper();
    while (s.size() > 3 && s.endsWith(u'\\'))
        s.chop(1);
    if (s.size() == 2 && s[1] == u':')
        s += u'\\';
    return s;
}

bool samePath(const QString& a, const QString& b)
{
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

bool isInside(const QString& child, const QString& parent)
{
    if (samePath(child, parent))
        return true;
    if (parent.endsWith(u'\\'))
        return child.startsWith(parent, Qt::CaseInsensitive);
    return child.size() > parent.size() && child[parent.size()] == u'\\' &&
           child.startsWith(parent, Qt::CaseInsensitive);
}

QString parentPath(const QString& path)
{
    const qsizetype sep = path.lastIndexOf(u'\\');
    if (sep < 0)
        return {};
    if (sep == path.size() - 1) // racine "C:\"
        return {};
    if (sep == 2 && path[1] == u':')
        return path.left(3);
    if (path.startsWith(u"\\\\") && path.indexOf(u'\\', 2) == sep)
        return {}; // \\serveur
    return path.left(sep);
}

QString lastSegment(const QString& path)
{
    if (path.size() == 3 && path[1] == u':')
        return path.left(2);
    const qsizetype sep = path.lastIndexOf(u'\\');
    return sep < 0 ? path : path.mid(sep + 1);
}

std::wstring toWin32(const QString& path)
{
    QString p = QDir::toNativeSeparators(path);
    if (p.size() >= 248 && !p.startsWith(u"\\\\?\\")) {
        if (p.startsWith(u"\\\\"))
            p = u"\\\\?\\UNC\\" + p.mid(2);
        else
            p = u"\\\\?\\" + p;
    }
    return p.toStdWString();
}

QString joinPath(const QString& dir, const QString& name)
{
    if (dir.endsWith(u'\\'))
        return dir + name;
    return dir + u'\\' + name;
}

} // namespace picazoo::platform
