#pragma once

// Copie suggérée à garder (006, research R2, R3) : la règle de qualité de la 002 (le centre du
// groupe), orientée par les règles de dossier, sans jamais écarter une copie RAW ou de plus grande
// résolution au profit d'une copie de dossier préféré dans un groupe qui n'est pas « Identique ».

#include "duplicates/DuplicateTypes.h"
#include "duplicates/FolderRules.h"
#include "duplicates/KeeperRule.h"

#include <QHash>
#include <QList>

namespace picazoo {

class Database;

struct SuggestionOutcome {
    ItemId keeper = 0;
    SuggestionReason reason = SuggestionReason::Quality;
    QString detail; // chemin de la règle qui a décidé
    int flags = 0;  // GroupFlag
};

class Suggestion {
public:
    // `copies` : toutes les copies du groupe (CopyFacts::path = chemin complet du fichier) ;
    // `center` : suggestion de la 002, gardée quand aucune règle ne s'applique ou ne doit
    // s'appliquer.
    static SuggestionOutcome choose(DupLevel level, ItemId center, const QList<CopyFacts>& copies,
                                    const FolderRules& rules);

    // Recalcule suggested_keep, suggestion_reason, suggestion_detail, flags et reclaimable_bytes
    // des groupes (tous si `only` est nul), puis retire les choix d'un groupe qui n'aurait plus
    // aucune copie gardée (FR-017). À appeler dans une transaction ouverte, connexion verrouillée.
    static void apply(Database& db, const QList<GroupId>* only = nullptr);

    // Faits des copies d'un ensemble de groupes, lus dans la base (même forme que le moteur).
    static QHash<GroupId, QList<CopyFacts>> loadFacts(Database& db, const QList<GroupId>* only);
    static FolderRules loadRules(Database& db);
};

// Place récupérable de tous les groupes, selon les choix et la suggestion.
extern const char* const kReclaimableAllSql;

} // namespace picazoo
