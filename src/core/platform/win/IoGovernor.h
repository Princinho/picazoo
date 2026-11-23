#pragma once

// Régulateur des lectures de fichiers de la 002 (research D5) : seau à jetons dont le débit
// s'adapte à la latence observée. Quand un bloc met plus de `congestionFactor` fois la
// latence de référence (une autre application sollicite le disque), le débit est divisé par
// deux ; il remonte progressivement au calme. Logique pure, horloge injectable (tests).

#include <QtGlobal>

#include <deque>
#include <functional>
#include <mutex>

namespace picazoo::platform {

class IoGovernor {
public:
    struct Config {
        double initialBytesPerSec = 16.0 * 1024 * 1024;
        double minBytesPerSec = 2.0 * 1024 * 1024;
        double maxBytesPerSec = 32.0 * 1024 * 1024;
        double congestionFactor = 3.0;   // latence > 3 × référence -> encombrement…
        double congestionFloorUs = 10000; // …et > 10 ms par Mo (ignore les à-coups d'un SSD)
        double recoveryPerSec = 0.25;    // +25 % par seconde calme
        qint64 minCutIntervalUs = 500000; // pas deux divisions en moins de 0,5 s
        int baselineWindow = 64;          // échantillons pour la latence de référence
    };
    using Clock = std::function<qint64()>; // microsecondes, monotone

    IoGovernor();
    explicit IoGovernor(Config config, Clock clock = {});

    // Avant de lire `bytes` : durée d'attente en microsecondes (0 si les jetons suffisent).
    // Les jetons sont consommés ; l'appelant attend la durée renvoyée.
    qint64 reserve(qint64 bytes);
    // Après la lecture : durée effective du bloc, pour détecter l'encombrement.
    void recordRead(qint64 bytes, qint64 latencyUs);

    double bytesPerSecond() const;
    // Sans plafond (vérification demandée par l'utilisateur) ; la latence reste observée.
    void setUnlimited(bool unlimited);
    bool unlimited() const;
    qint64 now() const { return clock_(); }

private:
    void refillLocked(qint64 now);
    double baselineLocked() const;

    Config cfg_;
    Clock clock_;
    mutable std::mutex mutex_;
    double rate_;
    double tokens_;
    qint64 lastRefill_ = 0;
    qint64 lastCut_ = 0;
    qint64 lastRecovery_ = 0;
    bool unlimited_ = false;
    std::deque<double> samples_; // latence par Mo, en µs
};

} // namespace picazoo::platform
