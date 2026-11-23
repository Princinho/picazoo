#include "platform/win/IoGovernor.h"

#include <QElapsedTimer>

#include <algorithm>
#include <vector>

namespace picazoo::platform {

namespace {

qint64 monotonicUs()
{
    static QElapsedTimer timer = [] {
        QElapsedTimer t;
        t.start();
        return t;
    }();
    return timer.nsecsElapsed() / 1000;
}

} // namespace

IoGovernor::IoGovernor() : IoGovernor(Config{}) {}

IoGovernor::IoGovernor(Config config, Clock clock)
    : cfg_(config), clock_(clock ? std::move(clock) : Clock(monotonicUs)), rate_(config.initialBytesPerSec),
      tokens_(config.initialBytesPerSec / 4) // petit élan initial (250 ms)
{
    lastRefill_ = clock_();
    lastRecovery_ = lastRefill_;
}

void IoGovernor::refillLocked(qint64 now)
{
    const qint64 dt = std::max<qint64>(0, now - lastRefill_);
    lastRefill_ = now;
    tokens_ = std::min(tokens_ + rate_ * double(dt) / 1e6, rate_); // au plus 1 s d'avance
}

qint64 IoGovernor::reserve(qint64 bytes)
{
    std::lock_guard lock(mutex_);
    if (unlimited_)
        return 0;
    const qint64 now = clock_();
    refillLocked(now);
    tokens_ -= double(bytes);
    if (tokens_ >= 0)
        return 0;
    return qint64(-tokens_ / rate_ * 1e6);
}

double IoGovernor::baselineLocked() const
{
    if (samples_.empty())
        return 0;
    // Référence robuste : 20e centile des derniers échantillons (latence « au calme »).
    std::vector<double> v(samples_.begin(), samples_.end());
    const size_t k = v.size() / 5;
    std::nth_element(v.begin(), v.begin() + qsizetype(k), v.end());
    return v[k];
}

void IoGovernor::recordRead(qint64 bytes, qint64 latencyUs)
{
    if (bytes <= 0)
        return;
    std::lock_guard lock(mutex_);
    const double perMb = double(latencyUs) * (1024.0 * 1024.0) / double(bytes);
    const double baseline = baselineLocked();
    const qint64 now = clock_();
    const bool enoughSamples = samples_.size() >= 8;
    const bool congested = enoughSamples && baseline > 0 && perMb > cfg_.congestionFactor * baseline &&
                           perMb > cfg_.congestionFloorUs;

    samples_.push_back(perMb);
    while (int(samples_.size()) > cfg_.baselineWindow)
        samples_.pop_front();

    if (congested) {
        if (now - lastCut_ >= cfg_.minCutIntervalUs) {
            rate_ = std::max(cfg_.minBytesPerSec, rate_ / 2);
            tokens_ = std::min(tokens_, 0.0);
            lastCut_ = now;
        }
        lastRecovery_ = now;
        return;
    }
    const qint64 dt = std::max<qint64>(0, now - lastRecovery_);
    lastRecovery_ = now;
    if (now - lastCut_ >= 1000000) // une seconde de calme avant de remonter
        rate_ = std::min(cfg_.maxBytesPerSec, rate_ * (1.0 + cfg_.recoveryPerSec * double(dt) / 1e6));
}

double IoGovernor::bytesPerSecond() const
{
    std::lock_guard lock(mutex_);
    return rate_;
}

void IoGovernor::setUnlimited(bool unlimited)
{
    std::lock_guard lock(mutex_);
    unlimited_ = unlimited;
}

bool IoGovernor::unlimited() const
{
    std::lock_guard lock(mutex_);
    return unlimited_;
}

} // namespace picazoo::platform
