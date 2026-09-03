#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

class AircraftDetectorProcess final
{
public:
    using Arguments = std::vector<std::string>;
    using Timeout = std::chrono::milliseconds;

    AircraftDetectorProcess(std::filesystem::path executable,
        Arguments arguments = {});
    ~AircraftDetectorProcess();

    AircraftDetectorProcess(const AircraftDetectorProcess&) = delete;
    AircraftDetectorProcess& operator=(const AircraftDetectorProcess&) = delete;

    bool Start();
    void Stop() noexcept;

    [[nodiscard]] bool Running() const noexcept;

private:
    std::filesystem::path m_executable;
    Arguments m_arguments;

    int m_processId{-1};
};