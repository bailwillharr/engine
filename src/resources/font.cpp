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
	auto fontBuffer = util::readBinaryFile(resPath.string());

	constexpr int FIRST_CHAR = 32;
	constexpr int NUM_CHARS = 96;

	constexpr int BITMAP_WIDTH = 1024;
	constexpr int BITMAP_HEIGHT = 1024;
	auto pixels = std::make_unique<unsigned char[]>(BITMAP_WIDTH * BITMAP_HEIGHT);
	auto bakedChars = std::make_unique<stbtt_bakedchar[]>(NUM_CHARS);

	stbtt_BakeFontBitmap(fontBuffer->data(), 0, 128.0f, pixels.get(), BITMAP_WIDTH, BITMAP_HEIGHT, FIRST_CHAR, NUM_CHARS, bakedChars.get());

	auto textureData = std::make_unique<uint8_t[]>(BITMAP_WIDTH * BITMAP_HEIGHT * 4);

	for (int i = 0; i < BITMAP_WIDTH * BITMAP_HEIGHT; i++) {
		textureData[i * 4 + 0] = pixels[i];
		textureData[i * 4 + 1] = pixels[i];
		textureData[i * 4 + 2] = pixels[i];
		textureData[i * 4 + 3] = pixels[i];
	}

	m_atlas = gfxdev->createTexture(textureData.get(), BITMAP_WIDTH, BITMAP_HEIGHT, gfx::TextureFilter::LINEAR, gfx::TextureFilter::LINEAR);

	for (int i = FIRST_CHAR; i < NUM_CHARS; i++) {
		CharData charData{};
		charData.atlas_top_left = { bakedChars[i].x0, bakedChars[i].y0 };
		charData.atlas_bottom_right = { bakedChars[i].x1, bakedChars[i].y1 };
		charData.offset = { bakedChars[i].xoff, bakedChars[i].yoff };
		charData.xAdvance = bakedChars[i].xadvance;
		m_charData[i] = charData;
		// TODO
	}

}

Font::~Font()
{
	gfxdev->destroyTexture(m_atlas);
}

const gfx::Texture* Font::getAtlasTexture()
{
	return m_atlas;
}

Font::CharData Font::getCharData(uint32_t charCode)
{
	return m_charData[charCode];
}

}
