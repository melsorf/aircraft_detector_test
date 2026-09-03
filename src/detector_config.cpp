#include "detector_config.hpp"

#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

template <typename T>
T Required(const YAML::Node& root, const char* name)
{
    const auto node = root[name];
    if (!node)
    {
        throw std::runtime_error("Missing configuration field: " + std::string(name));
    }

    try
    {
        return node.as<T>();
    }
    catch (const YAML::Exception& error)
    {
        throw std::runtime_error("Invalid configuration field '" + std::string(name) + "': " + error.what());
    }
}

} // namespace

namespace detector_config {

Detector::Config Load(const std::filesystem::path& path)
{
    YAML::Node root;
    try
    {
        root = YAML::LoadFile(path.string());
    }
    catch (const YAML::Exception& error)
    {
        throw std::runtime_error("Failed to load '" + path.string() + "': " + error.what());
    }

    return {
        .minSpeed = Required<double>(root, "min_speed"),
        .maxSpeed = Required<double>(root, "max_speed"),
        .minPoints = Required<std::size_t>(root, "min_points"),
        .lostTimeout = std::chrono::milliseconds{Required<std::int64_t>(root, "lost_timeout_ms")}
    };
}
} // namespace detector_config