#include "media/Formats.h"

#include <algorithm>
#include <array>
#include <utility>

namespace picazoo {

namespace {

struct ExtEntry {
    const char16_t* ext;
    Format format;
};

constexpr std::array<ExtEntry, 24> kExtensions{{
    {u"jpg", Format::Jpeg},  {u"jpeg", Format::Jpeg}, {u"jpe", Format::Jpeg},
    {u"png", Format::Png},   {u"gif", Format::Gif},   {u"bmp", Format::Bmp},
    {u"tif", Format::Tiff},  {u"tiff", Format::Tiff}, {u"webp", Format::Webp},
    {u"heic", Format::Heic}, {u"heif", Format::Heic},
    {u"cr2", Format::Cr2},   {u"cr3", Format::Cr3},   {u"nef", Format::Nef},
    {u"arw", Format::Arw},   {u"orf", Format::Orf},   {u"rw2", Format::Rw2},
    {u"raf", Format::Raf},   {u"dng", Format::Dng},
    {u"mp4", Format::Mp4},   {u"m4v", Format::Mp4},   {u"mov", Format::Mov},
    {u"avi", Format::Avi},   {u"mkv", Format::Mkv},
}};

constexpr std::array<std::pair<Format, const char16_t*>, 19> kNames{{
    {Format::Jpeg, u"jpeg"}, {Format::Png, u"png"},   {Format::Gif, u"gif"},   {Format::Bmp, u"bmp"},
    {Format::Tiff, u"tiff"}, {Format::Webp, u"webp"}, {Format::Heic, u"heic"}, {Format::Cr2, u"cr2"},
    {Format::Cr3, u"cr3"},   {Format::Nef, u"nef"},   {Format::Arw, u"arw"},   {Format::Orf, u"orf"},
    {Format::Rw2, u"rw2"},   {Format::Raf, u"raf"},   {Format::Dng, u"dng"},   {Format::Mp4, u"mp4"},
    {Format::Mov, u"mov"},   {Format::Avi, u"avi"},   {Format::Mkv, u"mkv"},
}};

} // namespace

Format formatFromPath(QStringView path)
{
    const qsizetype dot = path.lastIndexOf(u'.');
    if (dot < 0 || dot == path.size() - 1)
        return Format::Unknown;
    const qsizetype sep = std::max(path.lastIndexOf(u'\\'), path.lastIndexOf(u'/'));
    if (sep > dot)
        return Format::Unknown;
    const QStringView ext = path.mid(dot + 1);
    for (const auto& e : kExtensions) {
        if (ext.compare(QStringView(e.ext), Qt::CaseInsensitive) == 0)
            return e.format;
    }
    return Format::Unknown;
}

Kind kindOf(Format f)
{
    switch (f) {
    case Format::Cr2: case Format::Cr3: case Format::Nef: case Format::Arw:
    case Format::Orf: case Format::Rw2: case Format::Raf: case Format::Dng:
        return Kind::Raw;
    case Format::Mp4: case Format::Mov: case Format::Avi: case Format::Mkv:
        return Kind::Video;
    default:
        return Kind::Image;
    }
}

QString formatName(Format f)
{
    for (const auto& [format, name] : kNames) {
        if (format == f)
            return QString::fromUtf16(name);
    }
    return QStringLiteral("unknown");
}

Format formatFromName(const QString& s)
{
    for (const auto& [format, name] : kNames) {
        if (s == QStringView(name))
            return format;
    }
    return Format::Unknown;
}

bool isSupported(Format f) { return f != Format::Unknown; }

} // namespace picazoo
