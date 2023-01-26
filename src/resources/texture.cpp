#include "resources/texture.hpp"

#include "util/files.hpp"
#include "log.hpp"

#include <vector>

namespace engine::resources {

Texture::Texture(GFXDevice* gfxDevice, const std::string& path, Filtering filtering, bool useMipmaps, bool useLinearMagFilter)
	: m_gfxDevice(gfxDevice)
{

	int width, height;
	auto texbuf = util::readImageFile(path, &width, &height);

	gfx::TextureFilter minFilter = gfx::TextureFilter::NEAREST;
	gfx::TextureFilter magFilter = gfx::TextureFilter::NEAREST;
	gfx::MipmapSetting mipmapSetting = gfx::MipmapSetting::OFF;
	bool anisotropyEnable = false;

	if (useLinearMagFilter) {
		magFilter = gfx::TextureFilter::LINEAR;
	} else {
		magFilter = gfx::TextureFilter::NEAREST;
	}

	switch (filtering) {
		case Filtering::OFF:
			minFilter = gfx::TextureFilter::NEAREST;
			mipmapSetting = gfx::MipmapSetting::NEAREST;
			anisotropyEnable = false;
			break;
		case Filtering::BILINEAR:
			minFilter = gfx::TextureFilter::LINEAR;
			mipmapSetting = gfx::MipmapSetting::NEAREST;
			anisotropyEnable = false;
			break;
		case Filtering::TRILINEAR:
			minFilter = gfx::TextureFilter::LINEAR;
			mipmapSetting = gfx::MipmapSetting::LINEAR;
			anisotropyEnable = false;
			break;
		case Filtering::ANISOTROPIC:
			minFilter = gfx::TextureFilter::LINEAR;
			mipmapSetting = gfx::MipmapSetting::LINEAR;
			anisotropyEnable = true;
	}

	if (useMipmaps == false) {
		mipmapSetting = gfx::MipmapSetting::OFF;
	}

	m_gpuTexture = m_gfxDevice->createTexture(
		texbuf->data(), (uint32_t)width, (uint32_t)height,
		minFilter, magFilter,
		mipmapSetting,
		anisotropyEnable);

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
