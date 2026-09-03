#pragma once

#include <chrono>

class IClock
{
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = clock::duration;

    virtual ~IClock() = default;

    virtual time_point Now() const noexcept = 0;
};