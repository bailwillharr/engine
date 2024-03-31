#pragma once

#include <memory>
#include <string>
#include <vector>

namespace engine {
namespace util {

std::unique_ptr<std::vector<char>> ReadTextFile(const std::string& path);
std::unique_ptr<std::vector<uint8_t>> ReadBinaryFile(const std::string& path);

// Read an image file into a vector byte buffer. PNG and JPG support at a
// minimum. Output format is R8G8B8A8_UINT
std::unique_ptr<std::vector<uint8_t>> ReadImageFile(const std::string& path, int& width, int& height);

} // namespace util
} // namespace engine