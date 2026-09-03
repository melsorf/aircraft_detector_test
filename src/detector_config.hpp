#pragma once

#include "detector.hpp"

#include <filesystem>

namespace detector_config {

Detector::Config Load(const std::filesystem::path& path);

} // namespace detector_config