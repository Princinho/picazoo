#pragma once

namespace picazoo {

class Database;

// Applique les migrations numérotées de src/core/index/migrations/ (ressource Qt ":/sql").
// Renvoie la version du schéma après migration.
int migrateLibrary(Database& db);

constexpr int kLibrarySchemaVersion = 5; // 1 : 001 ; 2 : doublons (002) ; 3 : doublons en lot (006) ; 4 : origine des opérations (007) ; 5 : images liées (003)

} // namespace picazoo
