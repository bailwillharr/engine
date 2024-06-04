#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace engine {

std::filesystem::path openFileDialog(const std::vector<std::string>& extensions);

} // namespace engine