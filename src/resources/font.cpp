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

	constexpr int BITMAP_WIDTH = 1024;
	constexpr int BITMAP_HEIGHT = 1024;
	auto pixels = std::make_unique<unsigned char[]>(BITMAP_WIDTH * BITMAP_HEIGHT);
	auto chardata = std::make_unique<stbtt_bakedchar[]>(96);

	stbtt_BakeFontBitmap(fontBuffer->data(), 0, 128.0f, pixels.get(), BITMAP_WIDTH, BITMAP_HEIGHT, 32, 96, chardata.get());

	auto textureData = std::make_unique<uint8_t[]>(BITMAP_WIDTH * BITMAP_HEIGHT * 4);

	for (int i = 0; i < BITMAP_WIDTH * BITMAP_HEIGHT; i++) {
		textureData[i * 4 + 0] = pixels[i];
		textureData[i * 4 + 1] = pixels[i];
		textureData[i * 4 + 2] = pixels[i];
		textureData[i * 4 + 3] = pixels[i];
	}

	m_atlas = gfxdev->createTexture(textureData.get(), BITMAP_WIDTH, BITMAP_HEIGHT, gfx::TextureFilter::LINEAR, gfx::TextureFilter::LINEAR);

}

Font::~Font()
{
	gfxdev->destroyTexture(m_atlas);
}

const gfx::Texture* Font::getAtlasTexture()
{
	return m_atlas;
}

}
