#include "naming/NamingTypes.h"

namespace picazoo {

QString toDb(Confidence c)
{
    switch (c) {
    case Confidence::RegularSeries: return QStringLiteral("regular_series");
    case Confidence::SameRoot: return QStringLiteral("same_root");
    case Confidence::GenericRoot: return QStringLiteral("generic_root");
    }
    return QStringLiteral("same_root");
}

QString toDb(NamingGroupState s)
{
    switch (s) {
    case NamingGroupState::ToReview: return QStringLiteral("to_review");
    case NamingGroupState::Confirmed: return QStringLiteral("confirmed");
    case NamingGroupState::Rejected: return QStringLiteral("rejected");
    }
    return QStringLiteral("to_review");
}

Confidence confidenceFromDb(const QString& s)
{
    if (s == u"regular_series")
        return Confidence::RegularSeries;
    if (s == u"generic_root")
        return Confidence::GenericRoot;
    return Confidence::SameRoot;
}

NamingGroupState namingGroupStateFromDb(const QString& s)
{
    if (s == u"confirmed")
        return NamingGroupState::Confirmed;
    if (s == u"rejected")
        return NamingGroupState::Rejected;
    return NamingGroupState::ToReview;
}

QString confidenceLabel(Confidence c)
{
    switch (c) {
    case Confidence::RegularSeries: return QStringLiteral("Série régulière");
    case Confidence::SameRoot: return QStringLiteral("Même racine");
    case Confidence::GenericRoot: return QStringLiteral("Racine généralement utilisée par l'appareil");
    }
    return {};
}

} // namespace picazoo
