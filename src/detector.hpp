#pragma once

#include "clock.hpp"
#include "target_sink.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

class Detector final
{
public:
    struct Config
    {
        double minSpeed{};
        double maxSpeed{};
        std::size_t minPoints{};
        IClock::duration lostTimeout{};
    };

    struct Measurement
    {
        std::uint32_t trackId{};
        double x{};
        double y{};
        double z{};
    };

    Detector(IClock& clock, Config config);

    void SetSink(ITargetSink& sink) noexcept;

    void Process(const Measurement& measurement);

    void CheckTimeouts();

private:
    struct TrackState
    {
        Measurement first{};
        Measurement last{};

        IClock::time_point firstTime{};
        IClock::time_point lastTime{};

        std::size_t pointCount{};
        bool detected{};
    };

    void UpdateDetection(TrackState& track);

    [[nodiscard]] double CalculateSpeed(const TrackState& track) const noexcept;

    IClock& m_clock;
    Config m_config;
    ITargetSink* m_sink{};

    std::unordered_map<std::uint32_t, TrackState> m_tracks;
};