#pragma once

#include "SimulationFrame.hpp"

#include <string>

std::filesystem::path makeVtiFilePath(const std::filesystem::path &directory, int fileNumber);
SimulationFrame loadVtiFile(const std::filesystem::path &filePath);