#include "resources/texture.hpp"

#include "util/files.hpp"
#include "log.hpp"

#include <vector>

namespace engine::resources {

Texture::Texture(GFXDevice* gfxDevice, const std::string& path)
	: m_gfxDevice(gfxDevice)
{

	int width, height;
	auto texbuf = util::readImageFile(path, &width, &height);

	gfx::TextureFilter filter = gfx::TextureFilter::LINEAR;
	if (width <= 8 || height <= 8) {
		filter = gfx::TextureFilter::NEAREST;
	}

	m_gpuTexture = m_gfxDevice->createTexture(texbuf->data(), (uint32_t)width, (uint32_t)height, gfx::TextureFilter::LINEAR, filter);

	INFO("Loaded texture: {}, width: {} height: {}", path, width, height);

}

Texture::~Texture()
{
	m_gfxDevice->destroyTexture(m_gpuTexture);
}

gfx::Texture* Texture::getHandle()
{
	return m_gpuTexture;
}

}
