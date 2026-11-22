#include "video/VideoProbe.h"

#include <QEventLoop>
#include <QMediaPlayer>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

#include <future>

namespace picazoo {

class VideoProbeWorker : public QObject {
public:
    std::optional<VideoInfo> run(const QString& path, int timeoutMs)
    {
        QMediaPlayer player;
        QVideoSink sink;
        player.setVideoSink(&sink);
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

        VideoInfo info;
        bool seeked = false;
        bool failed = false;
        qint64 target = 0;

        QObject::connect(&player, &QMediaPlayer::errorOccurred, &loop, [&] {
            failed = true;
            loop.quit();
        });
        QObject::connect(&player, &QMediaPlayer::mediaStatusChanged, &loop, [&](QMediaPlayer::MediaStatus s) {
            if (s == QMediaPlayer::InvalidMedia) {
                failed = true;
                loop.quit();
            } else if (s == QMediaPlayer::LoadedMedia && !seeked) {
                seeked = true;
                const qint64 d = player.duration();
                if (d > 0)
                    info.durationMs = d;
                target = d > 0 ? d / 10 : 0;
                if (player.isSeekable() && target > 0)
                    player.setPosition(target);
                player.play();
            } else if (s == QMediaPlayer::EndOfMedia) {
                loop.quit();
            }
        });
        QObject::connect(&sink, &QVideoSink::videoFrameChanged, &loop, [&](const QVideoFrame& frame) {
            if (!seeked || !frame.isValid())
                return;
            // Après le positionnement, on accepte la première image proche de la cible.
            const qint64 t = frame.startTime() / 1000;
            if (target > 0 && t >= 0 && t + 1500 < target)
                return;
            QImage img = frame.toImage();
            if (!img.isNull()) {
                info.frame = img.convertToFormat(QImage::Format_RGB32);
                loop.quit();
            }
        });

        player.setSource(QUrl::fromLocalFile(path));
        timeout.start(timeoutMs);
        loop.exec();
        player.stop();
        if (failed || info.frame.isNull())
            return std::nullopt;
        if (!info.durationMs && player.duration() > 0)
            info.durationMs = player.duration();
        return info;
    }
};

VideoProbe::VideoProbe() : thread_(std::make_unique<QThread>())
{
    thread_->setObjectName(QStringLiteral("picazoo-video"));
    worker_ = new VideoProbeWorker;
    worker_->moveToThread(thread_.get());
    QObject::connect(thread_.get(), &QThread::finished, worker_, &QObject::deleteLater);
    thread_->start();
}

VideoProbe::~VideoProbe()
{
    thread_->quit();
    thread_->wait();
}

std::optional<VideoInfo> VideoProbe::probe(const QString& path, int timeoutMs)
{
    if (QThread::currentThread() == thread_.get())
        return worker_->run(path, timeoutMs);
    std::promise<std::optional<VideoInfo>> promise;
    auto future = promise.get_future();
    const bool queued = QMetaObject::invokeMethod(
        worker_, [&] { promise.set_value(worker_->run(path, timeoutMs)); }, Qt::QueuedConnection);
    if (!queued)
        return std::nullopt;
    // Le délai de chaque demande est borné par `timeoutMs` dans le thread vidéo ; une marge
    // couvre l'attente des demandes précédentes.
    return future.get();
}

QString formatDuration(qint64 ms)
{
    const qint64 s = std::max<qint64>(0, ms) / 1000;
    const qint64 h = s / 3600, m = (s % 3600) / 60, sec = s % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(sec, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m).arg(sec, 2, 10, QLatin1Char('0'));
}

} // namespace picazoo
