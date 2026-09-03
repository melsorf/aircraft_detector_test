#include "detector.hpp"
#include "detector_config.hpp"
#include "helpers/hlp_clock.hpp"
#include "helpers/hlp_sink.hpp"
#include "helpers/hlp_track.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <memory>

namespace
{

using namespace std::chrono_literals;

constexpr std::uint32_t testSeed = 42;
constexpr std::uint32_t trackId = 42;

std::filesystem::path DetectorConfig()
{
    const char* path = std::getenv("AIRCRAFT_DETECTOR_CONFIG");
    EXPECT_NE(path, nullptr);

    if (path == nullptr)
    {
        return {};
    }

    return path;
}

class DetectorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto configPath = DetectorConfig();
        ASSERT_FALSE(configPath.empty());

        m_config = detector_config::Load(configPath);
        ASSERT_NO_THROW(m_generator = std::make_unique<test::ScenarioGenerator>(testSeed, m_config));
        m_detector = std::make_unique<Detector>(m_clock, m_config);

        ASSERT_NO_THROW(m_detector->SetSink(m_sink));
    }

    Detector::Config m_config{};
    TestClock m_clock;
    TestSink m_sink;
    std::unique_ptr<test::ScenarioGenerator> m_generator;
    std::unique_ptr<Detector> m_detector;
};

struct InvalidMeasurement
{
    double x;
};

class InvalidMeasurementTest
    : public DetectorTest
    , public ::testing::WithParamInterface<InvalidMeasurement>
{
};

} // namespace

TEST_F(DetectorTest, DetectsUniformMotion)
{
    const auto track = m_generator->MovingTrack(trackId);

    test::ProcessTrack(*m_detector, m_clock, track);

    ASSERT_EQ(m_sink.m_statuses.size(), 1);

    const auto& status = m_sink.m_statuses.front();

    EXPECT_EQ(status.trackId, trackId);
    EXPECT_GE(status.speed, m_config.minSpeed);
    EXPECT_LE(status.speed, m_config.maxSpeed);
}

TEST_F(DetectorTest, DetectsNonUniformMotion)
{
    const auto interval = test::measurementInterval;
    const double seconds =
        std::chrono::duration<double>(interval).count();

    const double firstDistance = m_config.minSpeed * seconds;
    const double secondDistance = m_config.maxSpeed * seconds;

    const auto track = test::Track{
        trackId,
        {
            {0ms, {0.0, 0.0, 0.0}},
            {interval, {firstDistance, 0.0, 0.0}},
            {interval, {firstDistance + secondDistance, 0.0, 0.0}}
        }
    };

    test::ProcessTrack(*m_detector, m_clock, track);

    ASSERT_EQ(m_sink.m_statuses.size(), 1);

    const auto& status = m_sink.m_statuses.front();

    EXPECT_EQ(status.trackId, trackId);
    EXPECT_GE(status.speed, m_config.minSpeed);
    EXPECT_LE(status.speed, m_config.maxSpeed);
}

TEST_F(DetectorTest, DetectsTrackAtMinimumSpeed)
{
    const auto track = m_generator->MovingTrack(trackId, m_config.minSpeed);
    test::ProcessTrack(*m_detector, m_clock, track);

    ASSERT_EQ(m_sink.m_statuses.size(), 1);
    EXPECT_EQ(m_sink.m_statuses.front().trackId, trackId);
}

TEST_F(DetectorTest, DetectsTrackAtMaximumSpeed)
{
    const auto track = m_generator->MovingTrack(
        trackId,
        m_config.maxSpeed);

    test::ProcessTrack(*m_detector, m_clock, track);

    ASSERT_EQ(m_sink.m_statuses.size(), 1);
    EXPECT_EQ(m_sink.m_statuses.front().trackId, trackId);
}

TEST_F(DetectorTest, IgnoresTrackBelowMinimumSpeed)
{
    const double speed = m_config.minSpeed / 2.0;

    const auto track = m_generator->MovingTrack(
        trackId,
        speed);

    test::ProcessTrack(*m_detector, m_clock, track);

    EXPECT_EQ(m_sink.m_statuses.size(), 0);
}

TEST_F(DetectorTest, IgnoresTrackAboveMaximumSpeed)
{
    const double speed =
        m_config.maxSpeed +
        (m_config.maxSpeed - m_config.minSpeed);

    const auto track = m_generator->MovingTrack(
        trackId,
        speed);

    test::ProcessTrack(*m_detector, m_clock, track);

    EXPECT_EQ(m_sink.m_statuses.size(), 0);
}

TEST_F(DetectorTest, WaitsForMinimumNumberOfPoints)
{
    auto track = m_generator->MovingTrack(trackId);

    ASSERT_GE(track.measurements.size(), m_config.minPoints);

    track.measurements.resize(m_config.minPoints - 1);

    test::ProcessTrack(*m_detector, m_clock, track);

    EXPECT_EQ(m_sink.m_statuses.size(), 0);
}

TEST_F(DetectorTest, PublishesStatusOnlyOnce)
{
    const auto track = m_generator->MovingTrack(trackId);

    test::ProcessTrack(*m_detector, m_clock, track);

    ASSERT_EQ(m_sink.m_statuses.size(), 1);
    EXPECT_EQ(m_sink.m_statuses.front().trackId, trackId);
}

TEST_F(DetectorTest, PublishesLostAfterTimeout)
{
    const auto track = m_generator->MovingTrack(trackId);

    test::ProcessTrack(*m_detector, m_clock, track);

    ASSERT_EQ(m_sink.m_statuses.size(), 1);
    ASSERT_EQ(m_sink.m_lost.size(), 0);

    m_clock.Advance(m_config.lostTimeout + 1ms);
    m_detector->CheckTimeouts();

    ASSERT_EQ(m_sink.m_lost.size(), 1);
    EXPECT_EQ(m_sink.m_lost.front(), trackId);
}

TEST_F(DetectorTest, DoesNotPublishLostAtTimeout)
{
    const auto track = m_generator->MovingTrack(trackId);

    test::ProcessTrack(*m_detector, m_clock, track);

    ASSERT_EQ(m_sink.m_statuses.size(), 1);
    ASSERT_EQ(m_sink.m_lost.size(), 0);

    m_clock.Advance(m_config.lostTimeout);
    m_detector->CheckTimeouts();

    EXPECT_EQ(m_sink.m_lost.size(), 0);
}

TEST_F(DetectorTest, DoesNotPublishLostBeforeTimeout)
{
    const auto track = m_generator->MovingTrack(trackId);

    test::ProcessTrack(*m_detector, m_clock, track);

    ASSERT_EQ(m_sink.m_statuses.size(), 1);
    ASSERT_EQ(m_sink.m_lost.size(), 0);

    m_clock.Advance(m_config.lostTimeout / 2);
    m_detector->CheckTimeouts();

    EXPECT_EQ(m_sink.m_lost.size(), 0);
}

TEST_P(InvalidMeasurementTest, IgnoresInvalidSpeed)
{
    const auto invalid = GetParam();
    const auto interval = test::measurementInterval;

    const auto track = test::Track{
        trackId,
        {
            {0ms, {0.0, 0.0, 0.0}},
            {interval, {invalid.x, 0.0, 0.0}}
        }
    };

    test::ProcessTrack(*m_detector, m_clock, track);

    EXPECT_EQ(m_sink.m_statuses.size(), 0);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidInput,
    InvalidMeasurementTest,
    ::testing::Values(
        InvalidMeasurement{0.0},
        InvalidMeasurement{
            std::numeric_limits<double>::infinity()},
        InvalidMeasurement{
            std::numeric_limits<double>::quiet_NaN()}));
