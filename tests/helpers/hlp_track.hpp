#pragma once

#include "detector.hpp"
#include "helpers/hlp_clock.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace test
{

using Duration = std::chrono::milliseconds;

struct Position
{
    double x{};
    double y{};
    double z{};
};

struct Measurement
{
    Duration delay{};
    Position position;
};

struct Track
{
    std::uint32_t id{};
    std::vector<Measurement> measurements;
    double expectedSpeed{};
};

inline void ProcessTrack(
    Detector& detector,
    TestClock& clock,
    const Track& track)
{
    for (const auto& measurement : track.measurements)
    {
        clock.Advance(measurement.delay);

        detector.Process({
            .trackId = track.id,
            .x = measurement.position.x,
            .y = measurement.position.y,
            .z = measurement.position.z
        });
    }
}

inline constexpr auto measurementInterval =
    std::chrono::milliseconds{100};

class ScenarioGenerator
{
public:
    ScenarioGenerator(
        std::uint32_t seed,
        const Detector::Config& config)
        : m_random(seed)
        , m_config(config)
    {
    }

    [[nodiscard]] Track MovingTrack(std::uint32_t trackId)
    {
        std::uniform_real_distribution<double> speedDistribution(
            m_config.minSpeed,
            m_config.maxSpeed);

        return MovingTrack(trackId, speedDistribution(m_random));
    }

    [[nodiscard]] Track MovingTrack(
        std::uint32_t trackId,
        double speed)
    {
        const double timeStep =
            std::chrono::duration<double>(measurementInterval).count();

        const double distance = speed * timeStep;

        std::uniform_int_distribution<std::size_t> pointCountDistribution(
            m_config.minPoints,
            m_config.minPoints * 2);

        const auto pointCount = pointCountDistribution(m_random);

        std::uniform_real_distribution<double> coordinateDistribution(-100.0, 100.0);

        Position position{
            coordinateDistribution(m_random),
            coordinateDistribution(m_random),
            coordinateDistribution(m_random)
        };

        double directionX;
        double directionY;
        double directionZ;
        double directionLength;

        do
        {
            directionX = coordinateDistribution(m_random);
            directionY = coordinateDistribution(m_random);
            directionZ = coordinateDistribution(m_random);

            directionLength = std::sqrt(
                directionX * directionX +
                directionY * directionY +
                directionZ * directionZ);
        }
        while (directionLength == 0.0);

        directionX /= directionLength;
        directionY /= directionLength;
        directionZ /= directionLength;

        Track track{trackId, {}, speed};
        track.measurements.reserve(pointCount);

        track.measurements.push_back({
            Duration::zero(),
            position
        });

        for (std::size_t index = 1; index < pointCount; ++index)
        {
            position.x += directionX * distance;
            position.y += directionY * distance;
            position.z += directionZ * distance;

            track.measurements.push_back({
                measurementInterval,
                position
            });
        }

        return track;
    }

private:
    std::mt19937 m_random;
    const Detector::Config& m_config;
};

} // namespace test