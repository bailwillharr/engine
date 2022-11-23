#include "resources/font.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "util/files.hpp"
#include "gfx_device.hpp"

#include "log.hpp"

namespace engine::resources {

Font::Font(const std::filesystem::path& resPath) : Resource(resPath, "font")
{

	// TODO: load font
	auto fontBuffer = util::readBinaryFile(resPath);

	stbtt_fontinfo info{};
	int res = stbtt_InitFont(&info, fontBuffer->data(), 0);
	if (!res) {
		throw std::runtime_error("Failed to read font file: " + resPath.string());
	}

	float scale = stbtt_ScaleForPixelHeight(&info, 64);

	for (unsigned char c = 0; c < 128; c++) {

		// TODO: get character bitmap buffer, size, offsets, advance
		int32_t advance = 0, xoff = 0;
		stbtt_GetCodepointHMetrics(&info, c, &advance, &xoff);

		int32_t w, h, yoff;
		const uint8_t* bitmap = stbtt_GetCodepointBitmap(&info, scale, scale, c, &w, &h, &xoff, &yoff);

		DEBUG("char width: {} char height: {}", w, h);

		auto colorBuffer = std::make_unique<std::vector<uint32_t>>(w * h);
		int i = 0;
		for (uint32_t& col : *colorBuffer) {
			if (bitmap[i] == 0) {
				col = 0;
			} else {
				col = 0xFFFFFFFF;
			}
			i++;
		}

		// generate texture
		gfx::Texture* texture = gfxdev->createTexture(colorBuffer->data(), w, h, gfx::TextureFilter::LINEAR, gfx::TextureFilter::LINEAR);

		Character character = {
			texture,
			glm::ivec2{w, h},	// Size of Glyph
			glm::ivec2{xoff, yoff},		// Offset from baseline (bottom-left) to top-left of glyph
			advance
		};

		m_characters.insert(std::make_pair(c, character));

	}

	// TODO clean up resources

}

Font::~Font()
{
}

Font::Character Font::getChar(char c)
{
	return m_characters.at(c);
}

}
