#pragma once

// Inscription de la visionneuse auprès de Windows (research V11,
// contracts/storage-and-registry.md). Picazoo apparaît dans « Ouvrir avec » et
// dans « Applications par défaut », mais ne se déclare jamais application par
// défaut : seule une action de l'utilisateur dans les réglages de Windows le fait.

#include <QList>
#include <QString>
#include <QStringList>

namespace picazoo::platform {

enum class RegistryScope { User, Machine };

struct AssociationStatus {
    QString extension;  // ".jpg"
    bool isDefault = false; // Picazoo est l'application qui ouvre ce format
};

class FileAssociations {
public:
    static constexpr const wchar_t* kProgId = L"Picazoo.Image";
    static constexpr const wchar_t* kAppName = L"Picazoo";

    // Extensions inscrites (formats d'images de media/Formats).
    static QStringList extensions();

    // Écrit (ou retire) exactement les clés du contrat. `viewerExe` : chemin absolu de
    // picazoo-viewer.exe. La racine peut être redirigée pour les tests par la variable
    // d'environnement PICAZOO_REGISTRY_ROOT (sous-clé de HKCU, scope User seulement).
    static bool registerApp(RegistryScope scope, const QString& viewerExe, QString* error = nullptr);
    static bool unregisterApp(RegistryScope scope, QString* error = nullptr);
    static bool isRegistered();

    // Application effective de chaque extension (sans rien écrire).
    static QList<AssociationStatus> status(const QString& viewerExe);
    static bool isDefaultFor(const QString& extension, const QString& viewerExe);

    // Ouvre la page « Applications par défaut » de Windows sur Picazoo.
    static bool openDefaultAppsSettings();

    // Boîte « Ouvrir avec » de Windows pour un fichier (FR-025).
    static bool openWithDialog(const QString& path);

    // Racine de registre utilisée (tests et diagnostics), ex. « Software ».
    static QString baseKey(RegistryScope scope);
};

} // namespace picazoo::platform
