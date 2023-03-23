#pragma once

#include <memory>
#include <vector>
#include <string>

namespace engine::util {

	std::unique_ptr<std::vector<char>> readTextFile(const std::string& path);
	std::unique_ptr<std::vector<uint8_t>> readBinaryFile(const std::string& path);

	// Read an image file into a vector byte buffer. PNG and JPG support at a minimum.
	// Output format is R8G8B8A8_UINT
	std::unique_ptr<std::vector<uint8_t>> readImageFile(const std::string& path, int *width, int *height);

}
