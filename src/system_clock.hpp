#pragma once

#include "clock.hpp"

class SystemClock final : public IClock
{
public:
    time_point Now() const noexcept override
    {
        return clock::now();
    }
};