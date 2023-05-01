#include "util/files.h"

#include "stb_image.h"

#include <fstream>
#include <cstring>

namespace engine::util {

	std::unique_ptr<std::vector<char>> ReadTextFile(const std::string& path)
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

	std::unique_ptr<std::vector<uint8_t>> ReadBinaryFile(const std::string& path)
	{
		std::ifstream file(path, std::ios::ate | std::ios::binary);
		if (file.is_open() == false) {
			throw std::runtime_error("Unable to open file " + path);
		}

		auto buffer = std::make_unique<std::vector<uint8_t>>(file.tellg());
		
		file.seekg(0);

		file.read((char*)buffer->data(), buffer->size());

		file.close();

		return buffer;
	}

	std::unique_ptr<std::vector<uint8_t>> ReadImageFile(const std::string& path, int *width, int *height)
	{
		int x, y, n;
		unsigned char *data = stbi_load(path.c_str(), &x, &y, &n, STBI_rgb_alpha);

		if (data == nullptr) {
			throw std::runtime_error("Unable to open file " + path);
		}

		const size_t size = (size_t)x * (size_t)y * 4;

		auto buffer = std::make_unique<std::vector<uint8_t>>(size);
		memcpy(buffer->data(), data, buffer->size());

		*width = x;
		*height = y;

		stbi_image_free(data);

		return buffer;
	}

}

