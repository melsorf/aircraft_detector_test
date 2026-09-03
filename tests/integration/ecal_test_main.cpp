#include <ecal/ecal.h>
#include <gtest/gtest.h>

#include <cstdlib>

namespace {
class EcalGuard final
{
public:
    explicit EcalGuard(const char* applicationName)
        : m_initialized(eCAL::Initialize(applicationName))
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
    const bool m_initialized;
};

} // namespace

int main(int argc, char** argv)
{
    EcalGuard ecal{"aircraft_detector_integration_tests"};
    if (!ecal.IsInitialized())
    {
        return EXIT_FAILURE;
    }
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
