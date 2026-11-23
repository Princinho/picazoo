#pragma once

// Règles de dossier (006, research R1) : un dossier et ses sous-dossiers sont « à préférer » ou
// « à écarter ». Un dossier relève de la règle dont le chemin est son plus long préfixe, à une
// frontière de segment et sans tenir compte de la casse. Pur : aucune base, aucun fichier.

#include "duplicates/DuplicateTypes.h"

#include <QList>
#include <QString>

namespace picazoo {

class FolderRules {
public:
    FolderRules() = default;
    explicit FolderRules(QList<FolderRule> rules);

    // Règle la plus précise qui couvre ce dossier, ou nullptr.
    const FolderRule* match(const QString& folderPath) const;
    bool isEmpty() const { return rules_.isEmpty(); }
    const QList<FolderRule>& rules() const { return rules_; }

    // `rulePath` couvre-t-il `folderPath` (égal ou sous-dossier) ?
    static bool covers(const QString& rulePath, const QString& folderPath);
    // Chemin normalisé : séparateurs `\`, sans `\` final (sauf racine « C:\ »).
    static QString normalized(const QString& path);

private:
    QList<FolderRule> rules_; // du chemin le plus long au plus court
};

} // namespace picazoo
