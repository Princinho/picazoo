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

QString toDb(SuggestionReason r)
{
    switch (r) {
    case SuggestionReason::FolderPreferred: return QStringLiteral("folder_preferred");
    case SuggestionReason::FolderAvoided: return QStringLiteral("folder_avoided");
    case SuggestionReason::Quality: break;
    }
    return {};
}

SuggestionReason suggestionReasonFromDb(const QString& s)
{
    if (s == u"folder_preferred")
        return SuggestionReason::FolderPreferred;
    if (s == u"folder_avoided")
        return SuggestionReason::FolderAvoided;
    return SuggestionReason::Quality;
}

QString toDb(RuleKind k) { return k == RuleKind::Avoid ? QStringLiteral("avoid") : QStringLiteral("prefer"); }

RuleKind ruleKindFromDb(const QString& s) { return s == u"avoid" ? RuleKind::Avoid : RuleKind::Prefer; }

QString toDb(BatchKind k)
{
    switch (k) {
    case BatchKind::Reset: return QStringLiteral("reset");
    case BatchKind::KeepFolder: return QStringLiteral("keep_folder");
    case BatchKind::KeepAll: return QStringLiteral("keep_all");
    case BatchKind::NotDuplicate: return QStringLiteral("not_duplicate");
    }
    return QStringLiteral("reset");
}

BatchKind batchKindFromDb(const QString& s)
{
    if (s == u"keep_folder")
        return BatchKind::KeepFolder;
    if (s == u"keep_all")
        return BatchKind::KeepAll;
    if (s == u"not_duplicate")
        return BatchKind::NotDuplicate;
    return BatchKind::Reset;
}

QString suggestionLabel(const DuplicateGroup& g)
{
    switch (g.suggestionReason) {
    case SuggestionReason::FolderPreferred: return QStringLiteral("Dossier préféré : ") + g.suggestionDetail;
    case SuggestionReason::FolderAvoided: return QStringLiteral("Dossier à écarter : ") + g.suggestionDetail;
    case SuggestionReason::Quality: break;
    }
    return reasonLabel(g.reason);
}

QString flagLabel(int flags)
{
    if (flags & FlagPreferredWorse)
        return QStringLiteral("La copie du dossier préféré est moins bonne");
    if (flags & FlagAllAvoided)
        return QStringLiteral("Toutes les copies sont dans des dossiers à écarter");
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
