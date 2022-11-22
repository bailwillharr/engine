#include "util/files.hpp"

#include <fstream>

namespace engine::util {

	std::unique_ptr<std::vector<char>> readTextFile(const std::string& path)
	{
		auto buffer = std::make_unique<std::vector<char>>();

		std::ifstream file(path, std::ios::ate);

		if (file.is_open() == false) {
			throw std::runtime_error("Unable to open file " + path);
		}

		// reserve enough space for the text file, but leave the size at 0
		buffer->reserve(file.tellg()); 

		file.seekg(0);

		while (!file.eof()) {
			char c{};
			file.read(&c, 1);
			buffer->push_back(c);
		}

		file.close();

		return buffer;
	}

}
