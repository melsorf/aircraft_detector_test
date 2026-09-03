#include "ecal_adapter.hpp"

#include "radar.pb.h"

#include <string>

namespace {
constexpr const char* radarPointTopic = "RadarPoint";
constexpr const char* targetStatusTopic = "TargetStatus";
constexpr const char* targetLostTopic = "TargetLost";

template <typename Message>
bool Deserialize(const eCAL::SReceiveCallbackData& data, Message& message)
{
    return message.ParseFromArray(data.buffer, static_cast<int>(data.buffer_size));
}
} // namespace

EcalAdapter::EcalAdapter(Detector& detector)
    : m_detector(detector)
    , m_radarSubscriber(radarPointTopic)
    , m_statusPublisher(targetStatusTopic)
    , m_lostPublisher(targetLostTopic)
{
    m_radarSubscriber.SetReceiveCallback(
        [this](
            const eCAL::STopicId&/* publisher_id*/,
            const eCAL::SDataTypeInformation&/* type_info*/,
            const eCAL::SReceiveCallbackData& data) {
            HandleRadarPoint(data);
        });
}

void EcalAdapter::HandleRadarPoint(const eCAL::SReceiveCallbackData& data)
{
    RadarPoint point;
    if (!Deserialize(data, point))
    {
        return;
    }
    m_detector.Process({
        .trackId = point.track_id(),
        .x = point.x(),
        .y = point.y(),
        .z = point.z()
    });
}

void EcalAdapter::OnTargetStatus(std::uint32_t trackId, double speed)
{
    TargetStatus status;

    status.set_track_id(trackId);
    status.set_speed(speed);

    std::string payload;
    if (!status.SerializeToString(&payload))
    {
        return;
    }
    m_statusPublisher.Send(payload);
}

void EcalAdapter::OnTargetLost(std::uint32_t trackId)
{
    TargetLost lost;
    lost.set_track_id(trackId);
    std::string payload;
    if (!lost.SerializeToString(&payload))
    {
        return;
    }
    m_lostPublisher.Send(payload);
}