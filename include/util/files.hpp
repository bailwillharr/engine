#pragma once

#include <memory>
#include <vector>
#include <string>

namespace engine::util {

	std::unique_ptr<std::vector<char>> readTextFile(const std::string& path);
	std::unique_ptr<std::vector<uint8_t>> readBinaryFile(const std::string& path);

}
