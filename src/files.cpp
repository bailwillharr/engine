#include "files.h"

#include <fstream>
#include <cstring>

#include "stb_image.h"

namespace engine {

std::unique_ptr<std::vector<char>> readTextFile(const std::string& path)
{
    std::ifstream file(path, std::ios::ate);
    if (file.is_open() == false) {
        throw std::runtime_error("Unable to open file " + path);
    }

    auto buffer = std::make_unique<std::vector<char>>(static_cast<std::size_t>(file.tellg()) + 1);

    file.seekg(0);

    int i = 0;
    while (!file.eof()) {
        char c{};
        file.read(&c, 1); // reading 1 char at a time

        buffer->data()[i] = c;

        ++i;
    }

    // append zero byte
    // this used to be:
    // buffer->data()[buffer->size()] = '\0';
    // errors happened; i'm retarded
    buffer->data()[buffer->size() - 1] = '\0';

    file.close();

    return buffer;
}

std::unique_ptr<std::vector<uint8_t>> readBinaryFile(const std::string& path)
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

std::unique_ptr<std::vector<uint8_t>> readImageFile(const std::string& path, int& width, int& height)
{
    int x, y, n;
    unsigned char* data = stbi_load(path.c_str(), &x, &y, &n, STBI_rgb_alpha); // Image is 4 bpp

    if (data == nullptr) {
        throw std::runtime_error("Unable to open file " + path);
    }

    const std::size_t size = (std::size_t)x * (std::size_t)y * 4;

    auto buffer = std::make_unique<std::vector<uint8_t>>(size);
    memcpy(buffer->data(), data, buffer->size());

    width = x;
    height = y;

    stbi_image_free(data);

    return buffer;
}

} // namespace engine
