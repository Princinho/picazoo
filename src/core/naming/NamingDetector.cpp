#include "naming/NamingDetector.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace picazoo {

namespace {

// Forme 1 (research.md N1) : séparateur + chiffres en fin de nom. Le `.+` glouton capture
// la racine la plus longue possible, donc le séparateur le plus à droite qui précède des
// chiffres jusqu'à la fin (ex. "vacances_bali_001" -> "vacances_bali" + "_001").
const QRegularExpression& separatorDigitsRe()
{
    static const QRegularExpression re(QStringLiteral("^(.+)[_\\-\\s](\\d+)$"));
    return re;
}

// Forme 3 (research.md N1) : suffixe « copie » de l'Explorateur, avec ou sans numérotation
// entre parenthèses. Vérifiée avant la forme 2 (parenthèses génériques) pour que
// "photo - copie (2)" ne soit pas capturé comme "photo - copie" + "(2)" par la forme 2.
const QRegularExpression& copySuffixRe()
{
    static const QRegularExpression re(QStringLiteral("^(.+?)\\s*-\\s*copie(?:\\s*\\((\\d+)\\))?$"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re;
}

// Forme 2 (research.md N1) : numérotation entre parenthèses, avec ou sans espace avant.
const QRegularExpression& parenthesizedRe()
{
    static const QRegularExpression re(QStringLiteral("^(.+?)\\s*\\((\\d+)\\)$"));
    return re;
}

// Forme 4 (research.md N1) : chiffres collés en fin de nom, au moins 3 chiffres, pour éviter
// qu'un simple numéro de modèle en fin de nom ("img1") ne casse une racine autrement stable.
const QRegularExpression& gluedDigitsRe()
{
    static const QRegularExpression re(QStringLiteral("^(.*[^0-9])([0-9]{3,})$"));
    return re;
}

// Préfixes usuels donnés par un appareil photo ou un téléphone (research.md N2). Liste courte
// et quasi statique plutôt qu'une heuristique statistique, pour rester prévisible.
const QSet<QString>& genericDeviceTokens()
{
    static const QSet<QString> tokens{
        QStringLiteral("img"),        QStringLiteral("dsc"),   QStringLiteral("dscn"),
        QStringLiteral("pxl"),        QStringLiteral("vid"),   QStringLiteral("mvi"),
        QStringLiteral("gopr"),       QStringLiteral("capture"), QStringLiteral("screenshot"),
        QStringLiteral("image"),      QStringLiteral("photo"), QStringLiteral("video"),
        // Variantes courantes : Fuji, iPhone retouché, films, WhatsApp, Facebook, Signal,
        // captures d'écran de Windows en français.
        QStringLiteral("dcim"),       QStringLiteral("dscf"),  QStringLiteral("imge"),
        QStringLiteral("mov"),        QStringLiteral("imgwa"), QStringLiteral("vidwa"),
        QStringLiteral("fbimg"),      QStringLiteral("signal"),
        QStringLiteral("capturedécran"), QStringLiteral("capturedecran"),
    };
    return tokens;
}

// Racine condensée : seulement les lettres (séparateurs et chiffres retirés), en minuscule.
// Un numéro de date/heure au milieu de la racine ("PXL_20240714_153000") ne masque donc pas
// un préfixe d'appareil générique.
QString condensedAlpha(const QString& root)
{
    QString out;
    out.reserve(root.size());
    for (const QChar& c : root) {
        if (c.isLetter())
            out += c.toLower();
    }
    return out;
}

bool isGenericRoot(const QString& root)
{
    const QString condensed = condensedAlpha(root);
    if (condensed.size() < 3)
        return true; // racine trop courte ou peu spécifique (N2)
    return genericDeviceTokens().contains(condensed);
}

} // namespace

std::optional<DetectedRoot> detectRoot(const QString& fileNameWithoutExtension)
{
    if (fileNameWithoutExtension.isEmpty())
        return std::nullopt;

    QString root, digits;
    RootShape shape = RootShape::None;

    if (const auto m1 = separatorDigitsRe().match(fileNameWithoutExtension); m1.hasMatch()) {
        root = m1.captured(1);
        digits = m1.captured(2);
        shape = RootShape::SeparatorDigits;
    } else if (const auto m2 = copySuffixRe().match(fileNameWithoutExtension); m2.hasMatch()) {
        root = m2.captured(1);
        digits = m2.captured(2).isEmpty() ? QStringLiteral("1") : m2.captured(2);
        shape = RootShape::CopySuffix;
    } else if (const auto m3 = parenthesizedRe().match(fileNameWithoutExtension); m3.hasMatch()) {
        root = m3.captured(1);
        digits = m3.captured(2);
        shape = RootShape::Parenthesized;
    } else if (const auto m4 = gluedDigitsRe().match(fileNameWithoutExtension); m4.hasMatch()) {
        root = m4.captured(1);
        digits = m4.captured(2);
        shape = RootShape::GluedDigits;
    } else {
        return std::nullopt;
    }

    // Les séparateurs restés en fin de racine (« bali - 001 », « bali__001 ») n'en font pas partie.
    root = root.trimmed();
    while (!root.isEmpty() && (root.back() == u'_' || root.back() == u'-' || root.back().isSpace()))
        root.chop(1);
    if (root.isEmpty())
        return std::nullopt;

    DetectedRoot out;
    out.displayRoot = root;
    out.normalizedKey = root.toLower();
    out.shape = shape;
    out.generic = isGenericRoot(root);
    bool ok = false;
    if (const qint64 n = digits.toLongLong(&ok); ok)
        out.number = n;
    return out;
}

Confidence confidenceFor(const QList<std::optional<DetectedRoot>>& roots)
{
    if (roots.isEmpty())
        return Confidence::SameRoot;
    if (std::all_of(roots.begin(), roots.end(), [](const auto& r) { return r && r->generic; }))
        return Confidence::GenericRoot;

    // « Série régulière » : une seule racine, une seule forme, des numéros presque continus.
    QList<qint64> numbers;
    for (const auto& r : roots) {
        if (!r || !r->number || r->generic || r->shape != roots.first()->shape ||
            r->normalizedKey != roots.first()->normalizedKey)
            return Confidence::SameRoot;
        numbers << *r->number;
    }
    std::sort(numbers.begin(), numbers.end());
    numbers.erase(std::unique(numbers.begin(), numbers.end()), numbers.end());
    if (numbers.size() < 2)
        return Confidence::SameRoot;
    // Peu de « trous » : au plus un numéro manquant pour chaque numéro présent.
    return numbers.last() - numbers.first() < 2 * qint64(numbers.size()) ? Confidence::RegularSeries
                                                                         : Confidence::SameRoot;
}

QString suggestedName(const QString& displayRoot)
{
    QString name = displayRoot;
    name.replace(u'_', u' ');
    return name.simplified();
}

QString baseNameOf(const QString& fileName)
{
    const qsizetype dot = fileName.lastIndexOf(u'.');
    return dot > 0 ? fileName.left(dot) : fileName;
}

QList<NamingCandidate> groupByRoot(const QList<NamingInput>& items, bool includeGeneric)
{
    struct Bucket {
        QString displayRoot;
        QList<ItemId> items;
        QList<std::optional<DetectedRoot>> roots;
    };
    QHash<QString, Bucket> buckets;

    for (const auto& input : items) {
        const auto detected = detectRoot(input.fileName);
        if (!detected)
            continue;
        if (detected->generic && !includeGeneric)
            continue;
        Bucket& b = buckets[detected->normalizedKey];
        if (b.items.isEmpty())
            b.displayRoot = detected->displayRoot;
        b.items << input.item;
        b.roots << detected;
    }

    QList<NamingCandidate> out;
    for (auto it = buckets.constBegin(); it != buckets.constEnd(); ++it) {
        const auto& b = it.value();
        if (b.items.size() < 2)
            continue; // FR-004 : jamais de groupe d'un seul élément
        NamingCandidate c;
        c.rootKey = it.key();
        c.displayRoot = b.displayRoot;
        c.items = b.items;
        c.confidence = confidenceFor(b.roots);
        out << c;
    }
    return out;
}

} // namespace picazoo
