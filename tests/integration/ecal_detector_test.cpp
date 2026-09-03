#include "aircraft_detector_process.hpp"
#include "detector_config.hpp"
#include "ecal_test_bus.hpp"
#include "helpers/hlp_track.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <thread>

namespace {

using namespace std::chrono_literals;

constexpr auto startupTimeout = 5s;
constexpr auto messageTimeout = 2s;

constexpr std::uint32_t testSeed = 42;

[[nodiscard]] bool Publish(EcalTestBus& bus, const test::Track& track)
{
    for (const auto& measurement : track.measurements)
    {
        std::this_thread::sleep_for(measurement.delay);

        if (!bus.Send(test::ToRadarPoint(track.id, measurement.position)))
        {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool PublishInterleaved(
    EcalTestBus& bus,
    const test::Track& first,
    const test::Track& second)
{
    if (first.measurements.size() != second.measurements.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < first.measurements.size(); ++index)
    {
        const auto& firstMeasurement = first.measurements[index];
        const auto& secondMeasurement = second.measurements[index];

        std::this_thread::sleep_for(firstMeasurement.delay);

        if (!bus.Send(test::ToRadarPoint(first.id, firstMeasurement.position)))
        {
            return false;
        }

        std::this_thread::sleep_for(secondMeasurement.delay);

        if (!bus.Send(test::ToRadarPoint(second.id, secondMeasurement.position)))
        {
            return false;
        }
    }

    return true;
}

} // namespace

class EcalDetectorTest : public ::testing::Test
{
protected:
    EcalDetectorTest()
        : m_process(DetectorExecutable(), {"--config", DetectorConfig().string()})
        , m_testConfig(detector_config::Load(DetectorConfig()))
        , m_generator(testSeed, m_testConfig)
    {
    }

    void SetUp() override
    {
        ASSERT_FALSE(DetectorExecutable().empty())
            << "AIRCRAFT_DETECTOR is not set";

        ASSERT_FALSE(DetectorConfig().empty())
            << "AIRCRAFT_DETECTOR_CONFIG is not set";

        ASSERT_TRUE(m_process.Start())
            << "Failed to start aircraft_detector";

        ASSERT_TRUE(m_bus.WaitForSubscriber("RadarPoint", startupTimeout))
            << "aircraft_detector didn't subscribe to RadarPoint";
    }

    void TearDown() override
    {
        m_process.Stop();
    }

    [[nodiscard]] static std::filesystem::path EnvironmentPath(const char* name)
    {
        if (const char* value = std::getenv(name); value != nullptr)
        {
            return value;
        }

        return {};
    }

    [[nodiscard]] static std::filesystem::path DetectorExecutable()
    {
        return EnvironmentPath("AIRCRAFT_DETECTOR");
    }

    [[nodiscard]] static std::filesystem::path DetectorConfig()
    {
        return EnvironmentPath("AIRCRAFT_DETECTOR_CONFIG");
    }

    EcalTestBus m_bus;
    AircraftDetectorProcess m_process;
    Detector::Config m_testConfig;
    test::ScenarioGenerator m_generator;
};

TEST_F(EcalDetectorTest, PublishesTargetStatusForValidTrack)
{
    const auto scenario = m_generator.MovingTrack(1/*trackId*/);

    ASSERT_GE(scenario.measurements.size(), m_testConfig.minPoints);
    ASSERT_TRUE(Publish(m_bus, scenario));
    const auto status = m_bus.WaitForStatus(scenario.id, messageTimeout);
    ASSERT_TRUE(status.has_value())
        << "No status received for track " << scenario.id;

    EXPECT_EQ(status->track_id(), scenario.id);
    EXPECT_NEAR(status->speed(), scenario.expectedSpeed, 3.0);
}

TEST_F(EcalDetectorTest, DoesNotPublishMessagesForTooFewPoints)
{
    const auto movingTrack = m_generator.MovingTrack(2/*trackId*/);

    ASSERT_GE(movingTrack.measurements.size(), m_testConfig.minPoints);
    ASSERT_GE(m_testConfig.minPoints, 2u);

    test::Track scenario{
        movingTrack.id,
        {
            movingTrack.measurements.begin(),
            movingTrack.measurements.begin() + static_cast<std::ptrdiff_t>(m_testConfig.minPoints - 1)
        }
    };

    ASSERT_EQ(scenario.measurements.size(), m_testConfig.minPoints - 1);
    ASSERT_TRUE(Publish(m_bus, scenario));
    EXPECT_FALSE(m_bus.WaitForStatus(scenario.id, messageTimeout).has_value());
    EXPECT_FALSE(m_bus.WaitForLost(scenario.id, std::chrono::duration_cast<std::chrono::milliseconds>(
        m_testConfig.lostTimeout) + messageTimeout).has_value());
}

TEST_F(EcalDetectorTest, DoesNotPublishStatusForStationaryTarget)
{
    test::Track scenario{3/*trackId*/, {}};
    scenario.measurements.reserve(m_testConfig.minPoints);
    for (std::size_t index = 0; index < m_testConfig.minPoints; ++index)
    {
        scenario.measurements.push_back({index == 0 ? 0ms : test::measurementInterval,
            {15.0, -20.0, 5.0}
        });
    }

    ASSERT_TRUE(Publish(m_bus, scenario));
    EXPECT_FALSE(m_bus.WaitForStatus(scenario.id, messageTimeout).has_value());
}

TEST_F(EcalDetectorTest, DoesNotPublishStatusForTooFastTrack)
{
    const double speed = m_testConfig.maxSpeed + (m_testConfig.maxSpeed - m_testConfig.minSpeed);
    const double timeStep = std::chrono::duration<double>(test::measurementInterval).count();
    const double distance = speed * timeStep;
    test::Track scenario{4/*trackId*/, {}};
    scenario.measurements.reserve(m_testConfig.minPoints);
    double z = 30.0;
    scenario.measurements.push_back({0ms,{10.0, 20.0, z}});

    for (std::size_t index = 1; index < m_testConfig.minPoints; ++index)
    {
        z += distance;

        scenario.measurements.push_back({test::measurementInterval,{10.0, 20.0, z}});
    }

    ASSERT_TRUE(Publish(m_bus, scenario));
    EXPECT_FALSE(m_bus.WaitForStatus(scenario.id, messageTimeout).has_value());
}

TEST_F(EcalDetectorTest, PublishesIndependentStatusesForMultipleTracks)
{
    const auto first = m_generator.MovingTrack(10/*trackId*/);
    const auto second = m_generator.MovingTrack(20/*trackId*/);

    for (const auto& track : {first, second})
    {
        ASSERT_TRUE(Publish(m_bus, track));

        const auto status =
            m_bus.WaitForStatus(track.id, messageTimeout);

        ASSERT_TRUE(status.has_value())
            << "No status received for track " << track.id;

        EXPECT_EQ(status->track_id(), track.id);
    }
}

TEST_F(EcalDetectorTest, ProcessesInterleavedTracksIndependently)
{
    const auto first = m_generator.MovingTrack(30/*trackId*/);
    const auto second = m_generator.MovingTrack(40/*trackId*/);

    ASSERT_TRUE(PublishInterleaved(m_bus, first, second));

    const auto firstStatus =
        m_bus.WaitForStatus(first.id, messageTimeout);

    const auto secondStatus =
        m_bus.WaitForStatus(second.id, messageTimeout);

    ASSERT_TRUE(firstStatus.has_value())
        << "No status received for track " << first.id;

    ASSERT_TRUE(secondStatus.has_value())
        << "No status received for track " << second.id;

    EXPECT_EQ(firstStatus->track_id(), first.id);
    EXPECT_EQ(secondStatus->track_id(), second.id);

    EXPECT_NEAR(firstStatus->speed(), first.expectedSpeed, 2.0);
    EXPECT_NEAR(secondStatus->speed(), second.expectedSpeed, 2.0);

    EXPECT_GE(firstStatus->speed(), m_testConfig.minSpeed);
    EXPECT_LE(firstStatus->speed(), m_testConfig.maxSpeed);

    EXPECT_GE(secondStatus->speed(), m_testConfig.minSpeed);
    EXPECT_LE(secondStatus->speed(), m_testConfig.maxSpeed);
}

TEST_F(EcalDetectorTest, PublishesTargetLostAfterTrackDisappears)
{
    const auto scenario = m_generator.MovingTrack(100/*trackId*/);

    ASSERT_TRUE(Publish(m_bus, scenario));
    ASSERT_TRUE(m_bus.WaitForStatus(scenario.id, messageTimeout).has_value());

    const auto lost = m_bus.WaitForLost(scenario.id,
        std::chrono::duration_cast<std::chrono::milliseconds>(m_testConfig.lostTimeout) + messageTimeout);

    ASSERT_TRUE(lost.has_value())
        << "No TargetLost received for track " << scenario.id;
    EXPECT_EQ(lost->track_id(), scenario.id);
}

TEST_F(EcalDetectorTest, DoesNotLoseTrackImmediatelyAfterLastMeasurement)
{
    const auto scenario = m_generator.MovingTrack(101/*trackId*/);

    ASSERT_TRUE(Publish(m_bus, scenario));
    ASSERT_TRUE(m_bus.WaitForStatus(scenario.id, messageTimeout).has_value());
    EXPECT_FALSE(
        m_bus.WaitForLost(scenario.id, std::min(
            50ms, std::chrono::duration_cast<std::chrono::milliseconds>(m_testConfig.lostTimeout / 2))).has_value());
}
