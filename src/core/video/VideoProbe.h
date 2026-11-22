#pragma once

// Vignette et durée d'une vidéo (research R11, T059) : QMediaPlayer sans affichage,
// sur un thread dédié avec sa boucle d'événements, branché sur un QVideoSink.
// Positionnement à 10 % de la durée, capture d'une image. Délai maximal par vidéo.

#include <QImage>
#include <QString>

#include <memory>
#include <optional>

class QThread;

namespace picazoo {

struct VideoInfo {
    QImage frame;
    std::optional<qint64> durationMs;
};

class VideoProbeWorker;

class VideoProbe {
public:
    VideoProbe();
    ~VideoProbe();
    VideoProbe(const VideoProbe&) = delete;
    VideoProbe& operator=(const VideoProbe&) = delete;

    // Bloquant, appelable depuis n'importe quel thread ; les demandes sont traitées une à une.
    std::optional<VideoInfo> probe(const QString& path, int timeoutMs = 5000);

private:
    std::unique_ptr<QThread> thread_;
    VideoProbeWorker* worker_ = nullptr; // vit dans thread_
};

// "m:ss" sous une heure, "h:mm:ss" au-delà.
QString formatDuration(qint64 ms);

} // namespace picazoo
