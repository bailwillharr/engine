#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>

#include <stb_truetype.h>

namespace engine {

class Font {
    std::unique_ptr<stbtt_fontinfo> m_font_info{};
    std::unique_ptr<std::vector<uint8_t>> m_font_buffer{};
    std::map<int, int> m_unicode_to_glyph{};

public:
    explicit Font(const std::string& path);
    Font(const Font&) = delete;

    ~Font();

    Font& operator=(const Font&) = delete;

    std::unique_ptr<std::vector<uint8_t>> getTextBitmap(const std::string& text, float height_px, int& width_out, int& height_out);

private:
    int getGlyphIndex(int unicode_codepoint);
};

} // namespace engine