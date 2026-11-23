#pragma once

namespace picazoo {

class Database;

// Applique les migrations numérotées de src/core/index/migrations/ (ressource Qt ":/sql").
// Renvoie la version du schéma après migration.
int migrateLibrary(Database& db);

constexpr int kLibrarySchemaVersion = 2; // 1 : 001 ; 2 : doublons (002)

} // namespace picazoo
