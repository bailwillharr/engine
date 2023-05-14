#include "resources/font.h"

#include <string>

#include <stb_truetype.h>

#include "log.h"
#include "util/files.h"

namespace engine::resources {

Font::Font(const std::string& path) {
  // auto font_buffer = util::ReadBinaryFile(path);

  //  constexpr int FIRST_CHAR = 32;
  //  constexpr int NUM_CHARS = 96;
  //  constexpr float SCALE = 128.0f;

  // TODO finish this

  LOG_INFO("Loaded font: {}", path);
}

Font::~Font() {}

}  // namespace engine::resources
