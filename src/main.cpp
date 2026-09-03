#include "detector.hpp"
#include "detector_config.hpp"
#include "ecal_adapter.hpp"
#include "system_clock.hpp"

#include <ecal/ecal.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

class EcalGuard final
{
public:
    explicit EcalGuard(const char* applicationName)
        : m_initialized(eCAL::Initialize(applicationName, eCAL::Init::Default))
    {
    }

    EcalGuard(const EcalGuard&) = delete;
    EcalGuard& operator=(const EcalGuard&) = delete;

    ~EcalGuard()
    {
        if (m_initialized)
        {
            eCAL::Finalize();
        }
    }

    [[nodiscard]] bool IsInitialized() const noexcept
    {
        return m_initialized;
    }

private:
    bool m_initialized;
};

std::filesystem::path ParseConfigPath(int argc, char** argv)
{
    if (argc != 3 || std::string(argv[1]) != "--config")
    {
        throw std::runtime_error(
            "Usage: aircraft_detector --config <detector.yaml>");
    }
    return argv[2];
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const auto configPath = ParseConfigPath(argc, argv);
        const auto config = detector_config::Load(configPath);

        EcalGuard ecal{"aircraft_detector"};

        if (!ecal.IsInitialized())
        {
            std::cerr << "Failed to initialize eCAL\n";
            return EXIT_FAILURE;
        }

        SystemClock clock;
        Detector detector(clock, config);
        EcalAdapter adapter(detector);

        detector.SetSink(adapter);

        while (eCAL::Ok())
        {
            detector.CheckTimeouts();
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }

        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "aircraft_detector: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}