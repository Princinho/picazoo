#pragma once

// Parcours d'un dossier pour la visionneuse autonome (data-model « FolderBrowser »).
// La photo ouverte est disponible immédiatement ; le listage puis l'ordre de
// l'Explorateur arrivent en arrière-plan sans changer la photo affichée. Le
// dossier est surveillé (ajouts, suppressions, renommages). Aucune écriture.

#include "browse/FolderListing.h"

#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>

#include <functional>
#include <optional>

namespace picazoo {

class FolderBrowser : public QObject {
    Q_OBJECT

public:
    enum class OrderSource { Natural, Explorer };
    // Renvoie les noms de fichiers du dossier dans l'ordre de la fenêtre de
    // l'Explorateur, ou rien. Appelé sur un thread d'arrière-plan.
    using OrderProvider = std::function<std::optional<QStringList>(const QString& folder)>;

    explicit FolderBrowser(QObject* parent = nullptr);
    ~FolderBrowser() override;

    void setOrderProvider(OrderProvider provider) { provider_ = std::move(provider); }
    // Délai au-delà duquel un ordre de l'Explorateur arrivé trop tard est ignoré.
    void setOrderTimeoutMs(int ms) { orderTimeoutMs_ = ms; }
    void setWatchDebounceMs(int ms) { debounce_.setInterval(ms); }

    // `deferOrder` : l'ordre de l'Explorateur n'est demandé qu'à l'appel de
    // startOrderQuery() (après le premier affichage, pour ne pas ralentir l'ouverture).
    void open(const QString& filePath, bool deferOrder = false);
    void startOrderQuery();

    QString folder() const { return folder_; }
    qsizetype count() const { return entries_.size(); }
    int currentIndex() const { return current_; }
    QString currentPath() const { return currentPath_; }
    std::optional<BrowseEntry> currentEntry() const;
    const QList<BrowseEntry>& entries() const { return entries_; }
    // Chemin à `delta` positions de la photo courante (avec boucle), vide si aucun.
    QString pathAt(int delta) const;
    OrderSource orderSource() const { return orderSource_; }
    bool listed() const { return listed_; }
    bool missingCurrent() const { return missingCurrent_; }

    bool next();
    bool previous();
    void first();
    void last();
    void goTo(int index);

    // Tests : traite les événements jusqu'à la fin du listage (et de l'ordre si demandé).
    bool waitForListing(int timeoutMs, bool includeOrder = false);

signals:
    void currentChanged();   // la photo affichée change
    void listChanged();      // le parcours (taille, ordre, position) change
    void wrapped(bool forward); // passage de la dernière à la première (ou l'inverse)
    void currentRemoved();   // la photo affichée a disparu du disque

private:
    void onListed(quint64 gen, const QList<BrowseEntry>& listing, bool rescan);
    void onOrder(quint64 gen, const std::optional<QStringList>& order);
    void rebuild();
    void rescan();
    void setCurrentIndex(int index);
    int indexOfPath(const QString& path) const;

    QThreadPool pool_;
    QFileSystemWatcher watcher_;
    QTimer debounce_;
    QElapsedTimer opened_;
    OrderProvider provider_;
    int orderTimeoutMs_ = 2000;

    quint64 gen_ = 0;
    QString folder_;
    BrowseEntry openedEntry_;
    QList<BrowseEntry> listing_;            // ordre naturel
    std::optional<QStringList> viewOrder_;  // ordre de l'Explorateur
    bool orderDone_ = false;
    bool orderPending_ = false;
    QList<BrowseEntry> entries_;
    int current_ = -1;
    QString currentPath_;
    bool listed_ = false;
    bool missingCurrent_ = false;
    OrderSource orderSource_ = OrderSource::Natural;
};

} // namespace picazoo
