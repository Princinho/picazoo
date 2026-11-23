#include "browse/FolderBrowser.h"

#include "browse/BrowseOrder.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QFileInfo>
#include <QPointer>

#include <algorithm>

namespace picazoo {

FolderBrowser::FolderBrowser(QObject* parent) : QObject(parent)
{
    pool_.setMaxThreadCount(2);
    debounce_.setSingleShot(true);
    debounce_.setInterval(300);
    connect(&debounce_, &QTimer::timeout, this, &FolderBrowser::rescan);
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, [this] { debounce_.start(); });
}

FolderBrowser::~FolderBrowser()
{
    pool_.clear();
    pool_.waitForDone();
}

void FolderBrowser::open(const QString& filePath, bool deferOrder)
{
    ++gen_;
    const quint64 gen = gen_;
    opened_.restart();
    openedEntry_ = entryForFile(filePath);
    const QString folder = parentFolder(openedEntry_.path);

    if (!folder_.isEmpty() && folder_.compare(folder, Qt::CaseInsensitive) != 0)
        watcher_.removePath(folder_);
    folder_ = folder;
    if (!watcher_.directories().contains(folder_))
        watcher_.addPath(folder_);

    listing_.clear();
    viewOrder_.reset();
    orderDone_ = !provider_;
    listed_ = false;
    missingCurrent_ = false;
    orderSource_ = OrderSource::Natural;
    entries_ = {openedEntry_};
    current_ = 0;
    currentPath_ = openedEntry_.path;
    emit currentChanged();
    emit listChanged();

    pool_.start([this, gen, folder] {
        bool ok = false;
        QList<BrowseEntry> listing = listFolderImages(folder, &ok);
        QMetaObject::invokeMethod(this, [this, gen, listing = std::move(listing)] { onListed(gen, listing, false); },
                                  Qt::QueuedConnection);
    });
    orderPending_ = provider_ != nullptr;
    if (!deferOrder)
        startOrderQuery();
}

void FolderBrowser::startOrderQuery()
{
    if (!orderPending_ || !provider_)
        return;
    orderPending_ = false;
    const quint64 gen = gen_;
    const QString folder = folder_;
    opened_.restart(); // le délai d'abandon court à partir de la demande
    pool_.start([this, gen, folder, provider = provider_] {
        std::optional<QStringList> order = provider(folder);
        QMetaObject::invokeMethod(this, [this, gen, order = std::move(order)] { onOrder(gen, order); },
                                  Qt::QueuedConnection);
    });
}

std::optional<BrowseEntry> FolderBrowser::currentEntry() const
{
    if (current_ < 0 || current_ >= entries_.size() || missingCurrent_)
        return std::nullopt;
    return entries_[current_];
}

QString FolderBrowser::pathAt(int delta) const
{
    const qsizetype n = entries_.size();
    if (n == 0 || current_ < 0)
        return {};
    const qsizetype i = ((current_ + delta) % n + n) % n;
    return entries_[i].path;
}

int FolderBrowser::indexOfPath(const QString& path) const
{
    for (qsizetype i = 0; i < entries_.size(); ++i) {
        if (entries_[i].path.compare(path, Qt::CaseInsensitive) == 0)
            return int(i);
    }
    return -1;
}

void FolderBrowser::onListed(quint64 gen, const QList<BrowseEntry>& listing, bool rescan)
{
    if (gen != gen_)
        return;
    listing_ = listing;
    listed_ = true;
    Q_UNUSED(rescan);
    rebuild();
}

void FolderBrowser::onOrder(quint64 gen, const std::optional<QStringList>& order)
{
    if (gen != gen_)
        return;
    orderDone_ = true;
    if (!order || order->isEmpty() || opened_.elapsed() > orderTimeoutMs_)
        return;
    viewOrder_ = order;
    if (listed_)
        rebuild();
}

void FolderBrowser::rebuild()
{
    QList<BrowseEntry> base = listing_;
    // La photo ouverte peut ne pas être une image parcourable (format inconnu, fichier
    // caché) : elle reste dans le parcours tant qu'elle existe.
    const bool openedListed =
        std::any_of(base.cbegin(), base.cend(), [this](const BrowseEntry& e) {
            return e.path.compare(openedEntry_.path, Qt::CaseInsensitive) == 0;
        });
    if (!openedListed && QFileInfo::exists(openedEntry_.path)) {
        base.push_back(openedEntry_);
        sortNatural(base);
    }

    entries_ = viewOrder_ ? mergeOrder(*viewOrder_, base) : base;
    orderSource_ = viewOrder_ ? OrderSource::Explorer : OrderSource::Natural;

    const int idx = indexOfPath(currentPath_);
    if (idx >= 0) {
        current_ = idx;
        missingCurrent_ = false;
        emit listChanged();
        return;
    }
    // La photo affichée a disparu : la suivante prend sa place.
    current_ = entries_.isEmpty() ? -1 : std::clamp(current_, 0, int(entries_.size()) - 1);
    const bool wasMissing = missingCurrent_;
    missingCurrent_ = true;
    emit listChanged();
    if (!wasMissing)
        emit currentRemoved();
}

void FolderBrowser::rescan()
{
    if (folder_.isEmpty())
        return;
    if (!watcher_.directories().contains(folder_))
        watcher_.addPath(folder_);
    const quint64 gen = gen_;
    const QString folder = folder_;
    pool_.start([this, gen, folder] {
        bool ok = false;
        QList<BrowseEntry> listing = listFolderImages(folder, &ok);
        if (!ok)
            return;
        QMetaObject::invokeMethod(this, [this, gen, listing = std::move(listing)] { onListed(gen, listing, true); },
                                  Qt::QueuedConnection);
    });
}

void FolderBrowser::setCurrentIndex(int index)
{
    if (index < 0 || index >= entries_.size())
        return;
    const bool changed = missingCurrent_ || index != current_ ||
                         entries_[index].path.compare(currentPath_, Qt::CaseInsensitive) != 0;
    missingCurrent_ = false;
    current_ = index;
    currentPath_ = entries_[index].path;
    if (changed)
        emit currentChanged();
}

bool FolderBrowser::next()
{
    const int n = int(entries_.size());
    if (n == 0)
        return false;
    if (missingCurrent_) {
        setCurrentIndex(std::clamp(current_, 0, n - 1));
        return true;
    }
    if (n == 1)
        return false;
    int i = current_ + 1;
    if (i >= n) {
        i = 0;
        emit wrapped(true);
    }
    setCurrentIndex(i);
    return true;
}

bool FolderBrowser::previous()
{
    const int n = int(entries_.size());
    if (n == 0)
        return false;
    if (missingCurrent_) {
        setCurrentIndex(std::clamp(current_ - 1, 0, n - 1));
        return true;
    }
    if (n == 1)
        return false;
    int i = current_ - 1;
    if (i < 0) {
        i = n - 1;
        emit wrapped(false);
    }
    setCurrentIndex(i);
    return true;
}

void FolderBrowser::first() { setCurrentIndex(0); }

void FolderBrowser::last() { setCurrentIndex(int(entries_.size()) - 1); }

void FolderBrowser::goTo(int index) { setCurrentIndex(index); }

bool FolderBrowser::waitForListing(int timeoutMs, bool includeOrder)
{
    QDeadlineTimer deadline(timeoutMs);
    while (!(listed_ && (!includeOrder || orderDone_))) {
        if (deadline.hasExpired())
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QCoreApplication::sendPostedEvents();
    }
    return true;
}

} // namespace picazoo
