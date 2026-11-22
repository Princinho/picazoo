#include "library/Types.h"

namespace picazoo {

QString toDb(Kind k)
{
    switch (k) {
    case Kind::Image: return QStringLiteral("image");
    case Kind::Raw: return QStringLiteral("raw");
    case Kind::Video: return QStringLiteral("video");
    }
    return QStringLiteral("image");
}

QString toDb(ThumbState s)
{
    switch (s) {
    case ThumbState::Pending: return QStringLiteral("pending");
    case ThumbState::Provisional: return QStringLiteral("provisional");
    case ThumbState::Done: return QStringLiteral("done");
    case ThumbState::Unreadable: return QStringLiteral("unreadable");
    case ThumbState::NoPreview: return QStringLiteral("no_preview");
    }
    return QStringLiteral("pending");
}

QString toDb(MetaState s)
{
    switch (s) {
    case MetaState::Pending: return QStringLiteral("pending");
    case MetaState::Done: return QStringLiteral("done");
    case MetaState::Failed: return QStringLiteral("failed");
    }
    return QStringLiteral("pending");
}

QString toDb(RootState s)
{
    switch (s) {
    case RootState::Available: return QStringLiteral("available");
    case RootState::Unavailable: return QStringLiteral("unavailable");
    case RootState::Denied: return QStringLiteral("denied");
    }
    return QStringLiteral("available");
}

QString toDb(ThumbKind k)
{
    return k == ThumbKind::Final ? QStringLiteral("final") : QStringLiteral("provisional");
}

Kind kindFromDb(const QString& s)
{
    if (s == u"raw") return Kind::Raw;
    if (s == u"video") return Kind::Video;
    return Kind::Image;
}

ThumbState thumbStateFromDb(const QString& s)
{
    if (s == u"provisional") return ThumbState::Provisional;
    if (s == u"done") return ThumbState::Done;
    if (s == u"unreadable") return ThumbState::Unreadable;
    if (s == u"no_preview") return ThumbState::NoPreview;
    return ThumbState::Pending;
}

MetaState metaStateFromDb(const QString& s)
{
    if (s == u"done") return MetaState::Done;
    if (s == u"failed") return MetaState::Failed;
    return MetaState::Pending;
}

RootState rootStateFromDb(const QString& s)
{
    if (s == u"unavailable") return RootState::Unavailable;
    if (s == u"denied") return RootState::Denied;
    return RootState::Available;
}

} // namespace picazoo
