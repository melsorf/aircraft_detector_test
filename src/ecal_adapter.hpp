#pragma once

#include "detector.hpp"

#include <ecal/pubsub/publisher.h>
#include <ecal/pubsub/subscriber.h>

class EcalAdapter final : public ITargetSink
{
public:
    explicit EcalAdapter(Detector& detector);

    EcalAdapter(const EcalAdapter&) = delete;
    EcalAdapter& operator=(const EcalAdapter&) = delete;

    void OnTargetStatus(std::uint32_t track_id, double speed) override;
    void OnTargetLost(std::uint32_t track_id) override;

private:
    void HandleRadarPoint(const eCAL::SReceiveCallbackData& data);

    Detector& m_detector;

    eCAL::CSubscriber m_radarSubscriber;
    eCAL::CPublisher m_statusPublisher;
    eCAL::CPublisher m_lostPublisher;
};