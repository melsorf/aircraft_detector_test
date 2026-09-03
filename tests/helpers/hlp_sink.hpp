#pragma once

#include "target_sink.hpp"

#include <cstdint>
#include <vector>

class TestSink final : public ITargetSink
{
public:
    struct Status
    {
        std::uint32_t trackId{};
        double speed{};
    };

    void OnTargetStatus(std::uint32_t trackId, double speed) override
    {
        m_statuses.push_back({trackId, speed});
    }

    void OnTargetLost(std::uint32_t trackId) override
    {

        m_lost.push_back(trackId);
    }

    std::vector<Status> m_statuses;
    std::vector<std::uint32_t> m_lost;
};