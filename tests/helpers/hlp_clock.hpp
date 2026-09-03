#pragma once

#include "clock.hpp"

class TestClock final : public IClock
{
public:
    time_point Now() const noexcept override
    {
        return m_now;
    }

    void Advance(duration delta) noexcept
    {
        m_now += delta;
    }

private:
    time_point m_now{};
};