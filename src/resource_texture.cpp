#include "resource_texture.h"

#include "application.h"
#include "files.h"
#include "log.h"
#include "renderer.h"

#include <vector>

namespace engine {

Texture::Texture(Renderer* renderer, const uint8_t* bitmap, int width, int height, gfx::SamplerInfo samplerInfo, bool srgb) : gfx_(renderer->GetDevice())
{
    if (renderer->samplers.contains(samplerInfo) == false) {
        renderer->samplers.insert(std::make_pair(samplerInfo, gfx_->createSampler(samplerInfo)));
    }

    gfx::ImageFormat format = srgb ? gfx::ImageFormat::SRGB : gfx::ImageFormat::LINEAR;

    image_ = gfx_->createImage(width, height, format, bitmap);
    sampler_ = renderer->samplers.at(samplerInfo);

    LOG_DEBUG("Created texture: width: {}, height: {}", width, height);
}

Texture::~Texture()
{
    gfx_->destroyImage(image_);
    LOG_DEBUG("Destroyed texture");
}

std::unique_ptr<Texture> LoadTextureFromFile(const std::string& path, gfx::SamplerInfo samplerInfo, Renderer* renderer, bool srgb)
{
    int width, height;
    auto bitmap = readImageFile(path, width, height);
    return std::make_unique<Texture>(renderer, bitmap->data(), width, height, samplerInfo, srgb);
}

} // namespace engine