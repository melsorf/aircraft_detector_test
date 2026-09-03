#include "detector.hpp"

#include <cmath>
#include <stdexcept>

namespace {

double DistanceBetween(
    const Detector::Measurement& first,
    const Detector::Measurement& last) noexcept
{

    const double dx = last.x - first.x;
    const double dy = last.y - first.y;
    const double dz = last.z - first.z;

    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace

Detector::Detector(IClock& clock, Config config) : m_clock(clock), m_config(config)
{
    if (m_config.minSpeed < 0.0)
    {
        throw std::invalid_argument("min_speed must be non-negative");
    }

    if (m_config.maxSpeed <= m_config.minSpeed)
    {
        throw std::invalid_argument("max_speed must be greater than min_speed");
    }

    if (m_config.minPoints < 2)
    {
        throw std::invalid_argument("min_points must be at least 2");
    }

    if (m_config.lostTimeout <= IClock::duration::zero())
    {
        throw std::invalid_argument("lost_timeout must be positive");
    }
}

void Detector::SetSink(ITargetSink& sink) noexcept
{
    m_sink = &sink;
}

void Detector::Process(const Measurement& measurement)
{
    const auto now = m_clock.Now();
    auto [it, inserted] = m_tracks.try_emplace(measurement.trackId);
    auto& track = it->second;

    if (inserted)
    {
        track.first = measurement;
        track.last = measurement;
        track.firstTime = now;
        track.lastTime = now;
        track.pointCount = 1;
        return;
    }

    if (now <= track.lastTime)
    {
        return;
    }

    track.last = measurement;
    track.lastTime = now;
    ++track.pointCount;

    UpdateDetection(track);
}

void Detector::UpdateDetection(TrackState& track)
{
    if (track.detected || track.pointCount < m_config.minPoints)
    {
        return;
    }

    const double currentSpeed = CalculateSpeed(track);

    if (!std::isfinite(currentSpeed) ||
        currentSpeed < m_config.minSpeed ||
        m_config.maxSpeed < currentSpeed)
    {
        return;
    }

    track.detected = true;

    if (m_sink != nullptr)
    {
        m_sink->OnTargetStatus(track.last.trackId, currentSpeed);
    }
}

double Detector::CalculateSpeed(const TrackState& track) const noexcept
{
    const double seconds = std::chrono::duration<double>(track.lastTime - track.firstTime).count();
    if (seconds <= 0.0)
    {
        return 0.0;
    }

    return DistanceBetween(track.first, track.last) / seconds;
}

void Detector::CheckTimeouts()
{
    const auto now = m_clock.Now();
    for (auto it = m_tracks.begin(); it != m_tracks.end();)
    {
        const auto& track = it->second;
        if (now - track.lastTime <= m_config.lostTimeout)
        {
            ++it;
            continue;
        }

        if (track.detected && m_sink != nullptr)
        {
            m_sink->OnTargetLost(track.last.trackId);
        }
        it = m_tracks.erase(it);
    }
}