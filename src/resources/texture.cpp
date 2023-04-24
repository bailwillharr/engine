#include "resources/texture.hpp"

#include "application.hpp"
#include "util/files.hpp"
#include "log.hpp"

#include <vector>

namespace engine::resources {

Texture::Texture(RenderData* renderData, const std::string& path, Filtering filtering)
	: m_gfxDevice(renderData->gfxdev.get())
{

	int width, height;
	std::unique_ptr<std::vector<uint8_t>> texbuf = util::readImageFile(path, &width, &height);

	gfx::SamplerInfo samplerInfo{};

	samplerInfo.magnify = gfx::Filter::LINEAR;

	switch (filtering) {
		case Filtering::OFF:
			samplerInfo.minify = gfx::Filter::NEAREST;
			samplerInfo.mipmap = gfx::Filter::NEAREST;
			samplerInfo.anisotropicFiltering = false;
			break;
		case Filtering::BILINEAR:
			samplerInfo.minify = gfx::Filter::LINEAR;
			samplerInfo.mipmap = gfx::Filter::NEAREST;
			samplerInfo.anisotropicFiltering = false;
			break;
		case Filtering::TRILINEAR:
			samplerInfo.minify = gfx::Filter::LINEAR;
			samplerInfo.mipmap = gfx::Filter::LINEAR;
			samplerInfo.anisotropicFiltering = false;
			break;
		case Filtering::ANISOTROPIC:
			samplerInfo.minify = gfx::Filter::LINEAR;
			samplerInfo.mipmap = gfx::Filter::LINEAR;
			samplerInfo.anisotropicFiltering = true;
	}

	if (renderData->samplers.contains(samplerInfo) == false) {
		renderData->samplers.insert(std::make_pair(samplerInfo, m_gfxDevice->createSampler(samplerInfo)));
	}
	
	m_image = m_gfxDevice->createImage(width, height, texbuf->data());
	m_descriptorSet = m_gfxDevice->allocateDescriptorSet(renderData->materialSetLayout);
	m_gfxDevice->updateDescriptorCombinedImageSampler(m_descriptorSet, 0, m_image, renderData->samplers.at(samplerInfo));

	LOG_INFO("Loaded texture: {}, width: {} height: {}", path, width, height);

}

Texture::~Texture()
{
	m_gfxDevice->destroyImage(m_image);
}

}
