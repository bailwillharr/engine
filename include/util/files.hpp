#pragma once

#include <memory>
#include <vector>
#include <string>

namespace engine::util {

	std::unique_ptr<std::vector<char>> readTextFile(const std::string& path);

}
