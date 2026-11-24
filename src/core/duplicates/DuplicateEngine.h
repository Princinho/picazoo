#pragma once

// Moteur de détection des doublons (research D1–D7, D10). Un seul fil, en mode d'arrière-plan
// de Windows. Il ne travaille que si la bibliothèque (001) est inactive, hors pause, hors
// batterie et hors visionneuse active. L'avancement est déduit des tables : fermer
// l'application à tout moment ne perd que le lot en cours.
//
// Étapes : (1) tenir item_fingerprint à jour (versions, candidats exacts) ; (2) empreinte de
// contenu des candidats, lecture régulée ; (3) empreinte visuelle depuis les vignettes ;
// (4) recherche et vérification des paires visuelles ; (5) groupes en étoile.

#include "duplicates/DuplicateStore.h"
#include "duplicates/DuplicateTypes.h"
#include "duplicates/Grouper.h"
#include "duplicates/PairVerifier.h"

#include <QCache>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>

#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace picazoo {

class Database;
class Library;
namespace platform {
class IoGovernor;
}

class DuplicateEngine : public QObject {
    Q_OBJECT

public:
    struct Options {
        bool respectBattery = true; // pause sur batterie (FR-031) ; désactivé dans les tests
        bool autoStart = true;      // suit le réglage duplicates_enabled (FR-033)
    };

    explicit DuplicateEngine(Library& library, QObject* parent = nullptr);
    DuplicateEngine(Library& library, Options options, QObject* parent = nullptr);
    ~DuplicateEngine() override;

    void setEnabled(bool enabled); // réglage duplicates_enabled
    bool isEnabled() const { return enabled_.load(); }
    void pause();                  // FR-029 : plus aucune lecture
    void resume();
    bool isPaused() const { return userPaused_.load(); }
    void rescanAll();              // « Rechercher maintenant »
    void requestRegroup();         // après une décision ou une mise à la corbeille
    // Rappel à passer à Library::forgetItems (mise à la corbeille) : une fois les éléments
    // retirés de l'index, le moteur les oublie et relance la recherche, pour que groupes et
    // comptes reflètent ce qui reste. Sûr depuis n'importe quel fil, même moteur détruit.
    std::function<void()> forgetWhenRemoved(const QList<ItemId>& ids);
    void hold();                   // visionneuse active (cumulable)
    void release();

    DuplicateProgress progress() const;
    DuplicateStore& store() { return *store_; }
    Library& library() { return lib_; }

    // Tests : traite les événements jusqu'à ce que tout soit analysé et regroupé.
    bool waitForIdle(int timeoutMs);
    bool idle() const;

signals:
    void progressChanged();
    void groupsChanged();

private:
    struct Print {
        ItemId id = 0;
        int version = 1;
        std::array<quint64, 4> hash{};
        int width = 0, height = 0;
        std::optional<qint64> takenAt;
        QByteArray contentHash;
    };

    void loop();
    bool gatesOpen(QString* reason) const;
    void poke();
    bool syncRows();
    void dropPrints(const QSet<ItemId>& gone);
    bool contentStep();
    bool visualStep();
    void loadPrints();
    void search(const std::vector<Print>& news, bool againstAll);
    std::optional<VisualEdge> verifyPair(const Print& a, const Print& b, int rotation);
    std::optional<VisualSample> sampleFor(const Print& p);
    void regroup();
    int countPending();
    void flush();

    Library& lib_;
    Options options_;
    std::unique_ptr<DuplicateStore> store_;
    std::unique_ptr<Database> db_; // fil du moteur seulement
    std::unique_ptr<platform::IoGovernor> governor_;

    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> enabled_{true};
    std::atomic<bool> userPaused_{false};
    std::atomic<int> holds_{0};
    std::atomic<bool> workPending_{true};
    std::atomic<bool> regroupRequested_{true};
    std::atomic<bool> fullSearch_{true};
    std::atomic<bool> busy_{false};
    std::atomic<bool> libraryWasBusy_{true};
    // Éléments quittés la bibliothèque, à retirer des empreintes en mémoire (forgetWhenRemoved).
    // Partagé avec les rappels, qui peuvent survivre au moteur.
    struct ForgetInbox {
        std::mutex mutex;
        QSet<ItemId> ids;
    };
    std::shared_ptr<ForgetInbox> forgetInbox_ = std::make_shared<ForgetInbox>();

    // Progression (lue par l'interface)
    mutable std::mutex progressMutex_;
    DuplicateProgress progress_;
    std::atomic<bool> progressDirty_{false};
    std::atomic<bool> groupsDirty_{false};
    QTimer notifyTimer_;

    // État du fil du moteur
    std::vector<Print> prints_;
    QHash<ItemId, size_t> printIndex_;
    QHash<quint64, VisualEdge> edges_;
    QSet<quint64> rejected_;
    QSet<quint64> forbidden_;
    QCache<ItemId, VisualSample> samples_{2000};
    qint64 pendingTotal_ = 0;
    qint64 doneCount_ = 0;
};

} // namespace picazoo
