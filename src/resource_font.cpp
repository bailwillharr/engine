#include "resource_font.h"

#include <string>
#include <stdexcept>

#include <stb_truetype.h>

#include "log.h"
#include "files.h"

namespace engine {

Font::Font(const std::string& path)
{
    m_font_buffer = readBinaryFile(path);
    m_font_info = std::make_unique<stbtt_fontinfo>();

    if (stbtt_InitFont(m_font_info.get(), m_font_buffer->data(), 0) == 0) {
        throw std::runtime_error("Failed to initialise font!");
    }

    LOG_DEBUG("Created font: {}", path);
}

Font::~Font() { LOG_DEBUG("Destroyed font"); }

std::unique_ptr<std::vector<uint8_t>> Font::getTextBitmap(const std::string& text, float height_px, int& width_out, int& height_out)
{
    const float sf = stbtt_ScaleForPixelHeight(m_font_info.get(), height_px);

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(m_font_info.get(), &ascent, &descent, &line_gap);

    struct CharacterRenderInfo {
        int advance; // bitmap advance
        bool isEmpty;
        int xoff, yoff, width, height;
        unsigned char* bitmap;
    };

    std::vector<CharacterRenderInfo> characterRenderInfos{};

    int width = 0;
    for (size_t i = 0; i < text.size(); i++) {
        const int glyph_index = getGlyphIndex(static_cast<int>(text.at(i)));

        int advanceWidth, leftSideBearing;
        stbtt_GetGlyphHMetrics(m_font_info.get(), glyph_index, &advanceWidth, &leftSideBearing);

        if (i == 0 && leftSideBearing < 0) {
            // if the character extends before the current point
            width -= leftSideBearing;
        }
        width += advanceWidth;

        CharacterRenderInfo renderInfo{};

        renderInfo.advance = static_cast<int>(static_cast<float>(advanceWidth) * sf);

        if (stbtt_IsGlyphEmpty(m_font_info.get(), glyph_index) == 0) {
            renderInfo.isEmpty = false;
        }
        else {
            renderInfo.isEmpty = true;
        }

        if (!renderInfo.isEmpty) {
            renderInfo.bitmap =
                stbtt_GetGlyphBitmap(m_font_info.get(), sf, sf, glyph_index, &renderInfo.width, &renderInfo.height, &renderInfo.xoff, &renderInfo.yoff);
        }

        characterRenderInfos.push_back(renderInfo);
    }

    const size_t bitmap_width = static_cast<size_t>(std::ceil(static_cast<float>(width) * sf));
    const size_t bitmap_height = static_cast<size_t>(std::ceil(static_cast<float>(ascent - descent) * sf));
    auto bitmap = std::make_unique<std::vector<uint8_t>>(bitmap_width * bitmap_height * 4);

    for (size_t i = 0; i < bitmap->size() / 4; i++) {
        bitmap->at(i * 4 + 0) = 0x00;
        bitmap->at(i * 4 + 1) = 0x00;
        bitmap->at(i * 4 + 2) = 0x00;
        bitmap->at(i * 4 + 3) = 0x00;
    }

    int top_left_x = 0;
    for (const auto& renderInfo : characterRenderInfos) {
        if (renderInfo.isEmpty == false) {
            top_left_x += renderInfo.xoff;
            const int top_left_y = static_cast<int>(ascent * sf) + renderInfo.yoff;
            const int char_bitmap_width = renderInfo.width;
            const int char_bitmap_height = renderInfo.height;

            // copy the 8bpp char bitmap to the 4bpp output
            for (int y = 0; y < char_bitmap_height; y++) {
                for (int x = 0; x < char_bitmap_width; x++) {
                    const size_t out_index = ((bitmap_width * (static_cast<size_t>(top_left_y) + y)) + (static_cast<size_t>(top_left_x) + x)) * 4;
                    const uint8_t pixel = renderInfo.bitmap[y * char_bitmap_width + x];
                    (*bitmap)[out_index + 0] = pixel;
                    (*bitmap)[out_index + 1] = pixel;
                    (*bitmap)[out_index + 2] = pixel;
                    (*bitmap)[out_index + 3] = (pixel == 0x00) ? 0x00 : 0xFF;
                }
            }

            stbtt_FreeBitmap(renderInfo.bitmap, nullptr);
        }

        top_left_x += renderInfo.advance - renderInfo.xoff;
    }

    width_out = static_cast<int>(bitmap_width);
    height_out = static_cast<int>(bitmap_height);
    return bitmap;
}

int Font::getGlyphIndex(int unicode_codepoint)
{
    if (m_unicode_to_glyph.contains(unicode_codepoint)) {
        return m_unicode_to_glyph.at(unicode_codepoint);
    }
    else {
        const int glyph_index = stbtt_FindGlyphIndex(m_font_info.get(), unicode_codepoint);
        if (glyph_index == 0) {
            throw std::runtime_error("Glyph not found in font!");
        }
        m_unicode_to_glyph.emplace(std::make_pair(unicode_codepoint, glyph_index));
        return glyph_index;
    }
}

} // namespace engine
