#pragma once

namespace picazoo {

class Database;

// Applique les migrations numérotées de src/core/index/migrations/ (ressource Qt ":/sql").
// Renvoie la version du schéma après migration.
int migrateLibrary(Database& db);

constexpr int kLibrarySchemaVersion = 4; // 1 : 001 ; 2 : doublons (002) ; 3 : doublons en lot (006) ; 4 : origine des opérations (007)

} // namespace picazoo
