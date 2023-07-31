#include "util/files.h"

#include <fstream>
#include <cstring>

#include "stb_image.h"

namespace engine::util {

std::unique_ptr<std::vector<char>> ReadTextFile(const std::string& path) {
  std::ifstream file(path, std::ios::ate);
  if (file.is_open() == false) {
    throw std::runtime_error("Unable to open file " + path);
  }

  auto buffer = std::make_unique<std::vector<char>>(static_cast<size_t>(file.tellg()) + 1);

  file.seekg(0);

  int i = 0;
  while (!file.eof()) {
    char c{};
    file.read(&c, 1);  // reading 1 char at a time
    
    buffer->data()[i] = c;

    ++i;
  }

  // append zero byte
  buffer->data()[buffer->size()] = '\0';

  file.close();

  return buffer;
}

std::unique_ptr<std::vector<uint8_t>> ReadBinaryFile(const std::string& path) {
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

std::unique_ptr<std::vector<uint8_t>> ReadImageFile(const std::string& path,
                                                    int* width, int* height) {
  int x, y, n;
  unsigned char* data =
      stbi_load(path.c_str(), &x, &y, &n, STBI_rgb_alpha);  // Image is 4 bpp

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

}  // namespace engine::util
