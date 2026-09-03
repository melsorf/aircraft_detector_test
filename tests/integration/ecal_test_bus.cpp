#include "ecal_test_bus.hpp"

#include <algorithm>
#include <ranges>
#include <set>
#include <string>
#include <utility>

namespace {
constexpr const char* radarPointTopic = "RadarPoint";
constexpr const char* targetStatusTopic = "TargetStatus";
constexpr const char* targetLostTopic = "TargetLost";

constexpr EcalTestBus::Timeout radarPointDeliveryTimeout{1000};

template <typename Message>
bool Deserialize(const eCAL::SReceiveCallbackData& data, Message& message)
{
    return message.ParseFromArray(data.buffer, static_cast<int>(data.buffer_size));
}
} // namespace

namespace test {
RadarPoint ToRadarPoint(std::uint32_t trackId, const Position& position)
{
    RadarPoint point;

    point.set_track_id(trackId);
    point.set_x(position.x);
    point.set_y(position.y);
    point.set_z(position.z);

    return point;
}
} // namespace test

EcalTestBus::EcalTestBus()
    : m_radarPublisher(radarPointTopic)
    , m_radarSubscriber(radarPointTopic)
    , m_statusSubscriber(targetStatusTopic)
    , m_lostSubscriber(targetLostTopic)
{
    m_radarSubscriber.SetReceiveCallback(
        [this](
            const eCAL::STopicId& /* publisher_id */,
            const eCAL::SDataTypeInformation& /* type_info */,
            const eCAL::SReceiveCallbackData& data)
        {
            OnRadarPoint(data);
        });

    m_statusSubscriber.SetReceiveCallback(
        [this](
            const eCAL::STopicId& /* publisher_id */,
            const eCAL::SDataTypeInformation& /* type_info */,
            const eCAL::SReceiveCallbackData& data)
        {
            OnStatus(data);
        });

    m_lostSubscriber.SetReceiveCallback(
        [this](
            const eCAL::STopicId& /* publisher_id */,
            const eCAL::SDataTypeInformation& /* type_info */,
            const eCAL::SReceiveCallbackData& data)
        {
            OnLost(data);
        });
}

bool EcalTestBus::WaitForSubscriber(const std::string& topic, Timeout timeout)
{
    if (HasSubscriber(topic))
    {
        return true;
    }
    std::unique_lock lock(m_mutex);
    return m_condition.wait_for(lock, timeout,
        [this, &topic]
        {
            return HasSubscriber(topic);
        });
}

bool EcalTestBus::HasSubscriber(const std::string& topic) const
{
    std::set<eCAL::STopicId> subscribers;
    if (!eCAL::Registration::GetSubscriberIDs(subscribers))
    {
        return false;
    }
    return std::ranges::any_of(subscribers,
        [&topic](const auto& subscriber)
        {
            return subscriber.topic_name == topic;
        });
}

bool EcalTestBus::Send(const RadarPoint& point)
{
    std::string payload;
    if (!point.SerializeToString(&payload))
    {
        return false;
    }

    return m_radarPublisher.Send(payload);
}

std::optional<TargetStatus> EcalTestBus::WaitForStatus(std::uint32_t trackId, Timeout timeout)
{
    return WaitForMessage(m_statuses, trackId, timeout);
}

std::optional<TargetLost> EcalTestBus::WaitForLost(std::uint32_t trackId, Timeout timeout)
{
    return WaitForMessage(m_lost, trackId, timeout);
}

void EcalTestBus::Clear()
{
    std::lock_guard lock(m_mutex);

    m_statuses.clear();
    m_lost.clear();
}

void EcalTestBus::OnRadarPoint(const eCAL::SReceiveCallbackData& data)
{
    RadarPoint point;
    if (!Deserialize(data, point))
    {
        return;
    }
    m_condition.notify_all();
}

void EcalTestBus::OnStatus(const eCAL::SReceiveCallbackData& data)
{
    TargetStatus status;
    if (!Deserialize(data, status))
    {
        return;
    }

    {
        std::lock_guard lock(m_mutex);
        m_statuses.push_back(std::move(status));
    }
    m_condition.notify_all();
}

void EcalTestBus::OnLost(const eCAL::SReceiveCallbackData& data)
{
    TargetLost lost;
    if (!Deserialize(data, lost))
    {
        return;
    }

    {
        std::lock_guard lock(m_mutex);
        m_lost.push_back(std::move(lost));
    }
    m_condition.notify_all();
}
