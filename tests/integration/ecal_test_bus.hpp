#pragma once

#include "helpers/hlp_track.hpp"
#include "radar.pb.h"

#include <ecal/pubsub/publisher.h>
#include <ecal/pubsub/subscriber.h>
#include <ecal/registration.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace test {

[[nodiscard]] RadarPoint ToRadarPoint(std::uint32_t trackId, const Position& position);

}

class EcalTestBus
{
public:
    using Timeout = std::chrono::milliseconds;

    EcalTestBus();

    bool Send(const RadarPoint& point);

    bool WaitForSubscriber(const std::string& topic, Timeout timeout);

    std::optional<TargetStatus> WaitForStatus(std::uint32_t trackId, Timeout timeout);

    std::optional<TargetLost> WaitForLost(std::uint32_t trackId, Timeout timeout);

    void Clear();

private:
    template <typename Message>
    std::optional<Message> WaitForMessage(
        const std::vector<Message>& messages
        , std::uint32_t trackId
        , Timeout timeout)
    {
        std::unique_lock lock(m_mutex);
        auto it = messages.end();
        const bool received = m_condition.wait_for(
            lock,
            timeout,
            [&, trackId] {
                it = std::ranges::find_if(
                    messages,
                    [trackId](const auto& message) {
                        return message.track_id() == trackId;
                    });

                return it != messages.end();
            });

        return received ? std::optional<Message>{*it} : std::nullopt;
    }

    bool HasSubscriber(const std::string& topic) const;

    void OnRadarPoint(const eCAL::SReceiveCallbackData& data);

    void OnStatus(const eCAL::SReceiveCallbackData& data);

    void OnLost(const eCAL::SReceiveCallbackData& data);

    eCAL::CPublisher m_radarPublisher;
    eCAL::CSubscriber m_radarSubscriber;
    eCAL::CSubscriber m_statusSubscriber;
    eCAL::CSubscriber m_lostSubscriber;

    std::mutex m_mutex;
    std::condition_variable m_condition;

    std::size_t m_radarPointCount{0};

    std::vector<TargetStatus> m_statuses;
    std::vector<TargetLost> m_lost;
};