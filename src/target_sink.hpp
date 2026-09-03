#pragma once

#include <cstdint>

class ITargetSink
{
public:
    virtual ~ITargetSink() = default;

    virtual void OnTargetStatus(std::uint32_t trackId, double speed) = 0;

    virtual void OnTargetLost(std::uint32_t trackId) = 0;
};