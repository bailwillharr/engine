#include "resources/texture.hpp"

#include "util/files.hpp"
#include "log.hpp"

#include <vector>

namespace engine::resources {

Texture::Texture(GFXDevice* gfxDevice, const gfx::DescriptorSetLayout* materialSetLayout, const gfx::Sampler* sampler, const std::string& path, Filtering filtering, bool useMipmaps, bool useLinearMagFilter)
	: m_gfxDevice(gfxDevice)
{

	int width, height;
	std::unique_ptr<std::vector<uint8_t>> texbuf = util::readImageFile(path, &width, &height);

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

	(void)minFilter;
	(void)magFilter;
	(void)mipmapSetting;
	(void)anisotropyEnable;
	
	m_image = m_gfxDevice->createImage(width, height, texbuf->data());
	m_descriptorSet = m_gfxDevice->allocateDescriptorSet(materialSetLayout);
	m_gfxDevice->updateDescriptorCombinedImageSampler(m_descriptorSet, 0, m_image, sampler);

	LOG_INFO("Loaded texture: {}, width: {} height: {}", path, width, height);

}

Texture::~Texture()
{
	m_gfxDevice->destroyImage(m_image);
}

}
