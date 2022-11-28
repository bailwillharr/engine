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

	constexpr float SCALE = 128.0f;

	constexpr int BITMAP_WIDTH = 1024;
	constexpr int BITMAP_HEIGHT = 1024;
	auto pixels = std::make_unique<unsigned char[]>(BITMAP_WIDTH * BITMAP_HEIGHT);
	auto bakedChars = std::make_unique<stbtt_bakedchar[]>(NUM_CHARS);

	stbtt_BakeFontBitmap(fontBuffer->data(), 0, SCALE, pixels.get(), BITMAP_WIDTH, BITMAP_HEIGHT, FIRST_CHAR, NUM_CHARS, bakedChars.get());

	auto textureData = std::make_unique<uint8_t[]>(BITMAP_WIDTH * BITMAP_HEIGHT * 4);

	for (int i = 0; i < BITMAP_WIDTH * BITMAP_HEIGHT; i++) {
		textureData[i * 4 + 0] = 255;
		textureData[i * 4 + 1] = 255;
		textureData[i * 4 + 2] = 255;
		textureData[i * 4 + 3] = pixels[i];
	}

	m_atlas = gfxdev->createTexture(textureData.get(), BITMAP_WIDTH, BITMAP_HEIGHT, gfx::TextureFilter::LINEAR, gfx::TextureFilter::LINEAR);

	for (int i = FIRST_CHAR; i < FIRST_CHAR + NUM_CHARS; i++) {
		CharData charData{};
		stbtt_bakedchar baked = bakedChars[i - FIRST_CHAR];
		charData.atlas_top_left = { (float)baked.x0 / (float)BITMAP_WIDTH, (float)baked.y0 / (float)BITMAP_HEIGHT };
		charData.atlas_bottom_right = { (float)baked.x1 / (float)BITMAP_WIDTH, (float)baked.y1 / (float)BITMAP_HEIGHT};
		charData.char_top_left = { baked.xoff, baked.yoff };
		charData.char_bottom_right = charData.char_top_left + glm::vec2{ baked.x1 - baked.x0, baked.y1 - baked.y0 };
		charData.xAdvance = baked.xadvance;
		m_charData[i] = charData;
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
