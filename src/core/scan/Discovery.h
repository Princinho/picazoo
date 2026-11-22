#pragma once

// Découverte des photos et vidéos dans les dossiers surveillés (research R3, T037).
// Parcours en largeur ; un dossier n'est publié que s'il contient des fichiers pris en charge.

#include "library/Types.h"

#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

namespace picazoo {

struct RootSpec {
    RootId id = 0;
    QString path;
    QStringList excluded;
};

struct DiscoveredFile {
    QString name;
    qint64 size = 0;
    qint64 mtime = 0;
    Format format = Format::Unknown;
};

struct DiscoveredFolder {
    RootId rootId = 0;
    QString path;
    std::vector<DiscoveredFile> files;
};

struct DiscoveryCallbacks {
    std::function<void(const DiscoveredFolder&)> onFolder;
    std::function<void(const QString& path)> onFolderFailed; // sous-dossier illisible : ses éléments sont conservés
    std::function<bool()> cancelled;
    std::function<void()> waitIfPaused;
};

class Discovery {
public:
    // Retire les racines contenues dans une autre (parcourues une seule fois, R3) et
    // reporte leurs exclusions sur la racine englobante.
    static std::vector<RootSpec> effectiveRoots(std::vector<RootSpec> roots);

    // Parcourt une racine. Renvoie Available, Unavailable (introuvable) ou Denied.
    // `cancelled` interrompt le parcours ; l'appelant ne doit alors rien supprimer.
    static RootState walkRoot(const RootSpec& root, const DiscoveryCallbacks& cb);

    // Liste un seul dossier (non récursif). Vide si le dossier n'existe plus.
    static bool listFolder(RootId root, const QString& path, DiscoveredFolder& out, QStringList* subdirs = nullptr);

    static bool isExcluded(const QString& path, const QStringList& excluded);
};

} // namespace picazoo
