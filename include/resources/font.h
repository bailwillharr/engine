#ifndef ENGINE_INCLUDE_RESOURCES_FONT_H_
#define ENGINE_INCLUDE_RESOURCES_FONT_H_

#include <string>
#include <map>
#include <vector>
#include <memory>

#include <stb_truetype.h>

namespace engine {

class Font {
 public:
  Font(const std::string& path);
  ~Font();
  Font(const Font&) = delete;
  Font& operator=(const Font&) = delete;

  std::unique_ptr<std::vector<uint8_t>> GetTextBitmap(const std::string& text,
                                                      float height_px,
                                                      int& width_out,
                                                      int& height_out);

 private:
  std::unique_ptr<stbtt_fontinfo> font_info_{};
  std::unique_ptr<std::vector<uint8_t>> font_buffer_{};
  std::map<int, int> unicode_to_glyph_{};

  int GetGlyphIndex(int unicode_codepoint);
};

}  // namespace engine

#endif