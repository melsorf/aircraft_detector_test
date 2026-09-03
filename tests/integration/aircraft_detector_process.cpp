#include "aircraft_detector_process.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <thread>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

namespace {

constexpr auto stopTimeout = std::chrono::seconds{2};
constexpr auto pollInterval = std::chrono::milliseconds{10};

bool WaitForProcess(pid_t pid, std::chrono::steady_clock::time_point deadline) noexcept
{
    int status{};
    while (std::chrono::steady_clock::now() < deadline)
    {
        const pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid || (result == -1 && errno == ECHILD))
        {
            return true;
        }
        std::this_thread::sleep_for(pollInterval);
    }

    return false;
}

} // namespace

AircraftDetectorProcess::AircraftDetectorProcess(
    std::filesystem::path executable,
    Arguments arguments)
    : m_executable(std::move(executable))
    , m_arguments(std::move(arguments))
{
}

AircraftDetectorProcess::~AircraftDetectorProcess()
{
    Stop();
}

bool AircraftDetectorProcess::Start()
{
    if (Running())
    {
        return true;
    }

    const pid_t pid = fork();

    if (pid < 0)
    {
        return false;
    }

    if (pid == 0)
    {
        std::vector<char*> argv;
        argv.reserve(m_arguments.size() + 2);
        argv.push_back(const_cast<char*>(m_executable.c_str()));

        for (auto& argument : m_arguments)
        {
            argv.push_back(argument.data());
        }

        argv.push_back(nullptr);

        execv(m_executable.c_str(), argv.data());
        _exit(127);
    }

    m_processId = pid;
    return true;
}

void AircraftDetectorProcess::Stop() noexcept
{
    if (m_processId <= 0)
    {
        return;
    }
    int status{};
    const pid_t result = waitpid(m_processId, &status, WNOHANG);
    if (result == m_processId || (result == -1 && errno == ECHILD))
    {
        m_processId = -1;
        return;
    }
    if (kill(m_processId, SIGTERM) != 0 && errno == ESRCH)
    {
        m_processId = -1;
        return;
    }
    if (WaitForProcess(
            m_processId,
            std::chrono::steady_clock::now() + stopTimeout))
    {
        m_processId = -1;
        return;
    }

    kill(m_processId, SIGKILL);
    waitpid(m_processId, &status, 0);
    m_processId = -1;
}

bool AircraftDetectorProcess::Running() const noexcept
{
    if (m_processId <= 0)
    {
        return false;
    }

    int status{};
    const pid_t result = waitpid(m_processId, &status, WNOHANG);

    return result == 0;
}
