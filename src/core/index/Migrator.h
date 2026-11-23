#pragma once

namespace picazoo {

class Database;

// Applique les migrations numérotées de src/core/index/migrations/ (ressource Qt ":/sql").
// Renvoie la version du schéma après migration.
int migrateLibrary(Database& db);

constexpr int kLibrarySchemaVersion = 3; // 1 : 001 ; 2 : doublons (002) ; 3 : doublons en lot (006)

} // namespace picazoo
