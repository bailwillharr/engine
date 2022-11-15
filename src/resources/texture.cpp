#include "resources/texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <log.hpp>

#include <vector>

namespace engine::resources {

// returns false if unable to open file
static bool readPNG(const std::string& path, std::vector<uint8_t>* texbuf, int *width, int *height, bool *isRGBA)
{
	int x, y, n;
	unsigned char *data = stbi_load(path.c_str(), &x, &y, &n, STBI_rgb_alpha);


	if (data == nullptr) {
		return false;
	}

	const size_t size = (size_t)x * (size_t)y * 4;

	texbuf->resize(size);
	memcpy(texbuf->data(), data, size);

	*width = x;
	*height = y;
	*isRGBA = true;

	stbi_image_free(data);

	return true;

}

static bool readGLRaw(const std::string& path, std::vector<uint8_t>* texbuf, int *width, int *height, bool *isRGBA)
{
	FILE *fp = fopen(path.c_str(), "rb");
	if (!fp) {
		return false;
	}

	fseek(fp, 0x02, SEEK_SET);
	uint64_t tex_data_offset;
	fread(&tex_data_offset, sizeof(uint64_t), 1, fp);

		fseek(fp, 0L, SEEK_END);
	uint64_t end = ftell(fp);

	texbuf->resize(end);
	fseek(fp, (long)tex_data_offset, SEEK_SET);
	fread(texbuf->data(), 1, end, fp);

	fclose(fp);

	*width = 4096;
	*height = 4096;
	*isRGBA = false;

	return true;

}

Texture::Texture(const std::filesystem::path& resPath) : Resource(resPath, "texture")
{
	
	auto texbuf = std::make_unique<std::vector<uint8_t>>();

	int width, height;
	bool isRGBA, success;

	if (resPath.extension() == ".png" || resPath.extension() == ".jpg") {
		success = readPNG(resPath.string(), texbuf.get(), &width, &height, &isRGBA);
	} else {
		success = readGLRaw(resPath.string(), texbuf.get(), &width, &height, &isRGBA);
	}

	if (!success) {
		throw std::runtime_error("Unable to open texture: " + resPath.string());
	}

	if (isRGBA == false) {
		throw std::runtime_error("Currently, only RGBA textures are supported. Size: " + std::to_string(texbuf->size()));
	}

	gfx::TextureFilter filter = gfx::TextureFilter::LINEAR;
	if (width <= 8 || height <= 8) {
		filter = gfx::TextureFilter::NEAREST;
	}

	m_gpuTexture = gfxdev->createTexture(texbuf->data(), (uint32_t)width, (uint32_t)height, gfx::TextureFilter::LINEAR, filter);

	DEBUG("loaded texture {} width: {} height: {} size: {}", resPath.filename().string(), width, height, texbuf->size());

}

Texture::~Texture()
{
	gfxdev->destroyTexture(m_gpuTexture);
}

gfx::Texture* Texture::getHandle()
{
	return m_gpuTexture;
}

}
