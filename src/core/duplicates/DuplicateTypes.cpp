#include "duplicates/DuplicateTypes.h"

namespace picazoo {

QString toDb(DupLevel l)
{
    switch (l) {
    case DupLevel::Identical: return QStringLiteral("identical");
    case DupLevel::SameImage: return QStringLiteral("same_image");
    case DupLevel::Similar: return QStringLiteral("similar");
    }
    return QStringLiteral("identical");
}

QString toDb(KeeperReason r)
{
    switch (r) {
    case KeeperReason::Raw: return QStringLiteral("raw");
    case KeeperReason::Resolution: return QStringLiteral("resolution");
    case KeeperReason::LessCompressed: return QStringLiteral("less_compressed");
    case KeeperReason::HasExif: return QStringLiteral("has_exif");
    case KeeperReason::Oldest: return QStringLiteral("oldest");
    case KeeperReason::Path: return QStringLiteral("path");
    }
    return QStringLiteral("path");
}

QString toDb(GroupState s)
{
    return s == GroupState::KeptAll ? QStringLiteral("kept_all") : QStringLiteral("to_review");
}

DupLevel dupLevelFromDb(const QString& s)
{
    if (s == u"same_image")
        return DupLevel::SameImage;
    if (s == u"similar")
        return DupLevel::Similar;
    return DupLevel::Identical;
}

KeeperReason keeperReasonFromDb(const QString& s)
{
    if (s == u"raw")
        return KeeperReason::Raw;
    if (s == u"resolution")
        return KeeperReason::Resolution;
    if (s == u"less_compressed")
        return KeeperReason::LessCompressed;
    if (s == u"has_exif")
        return KeeperReason::HasExif;
    if (s == u"oldest")
        return KeeperReason::Oldest;
    return KeeperReason::Path;
}

GroupState groupStateFromDb(const QString& s)
{
    return s == u"kept_all" ? GroupState::KeptAll : GroupState::ToReview;
}

QString levelLabel(DupLevel l)
{
    switch (l) {
    case DupLevel::Identical: return QStringLiteral("Identique");
    case DupLevel::SameImage: return QStringLiteral("Même image");
    case DupLevel::Similar: return QStringLiteral("Très ressemblante");
    }
    return {};
}

QString reasonLabel(KeeperReason r)
{
    switch (r) {
    case KeeperReason::Raw: return QStringLiteral("RAW d'origine");
    case KeeperReason::Resolution: return QStringLiteral("Plus grande résolution");
    case KeeperReason::LessCompressed: return QStringLiteral("Moins compressée");
    case KeeperReason::HasExif: return QStringLiteral("Informations de prise de vue");
    case KeeperReason::Oldest: return QStringLiteral("La plus ancienne");
    case KeeperReason::Path: return QStringLiteral("Chemin le plus court");
    }
    return {};
}

} // namespace picazoo
