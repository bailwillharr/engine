#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace engine::util {

std::filesystem::path OpenFileDialog(const std::vector<std::string>& extensions);

} // namespace engine::util