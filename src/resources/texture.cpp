#include "resources/texture.h"

#include "application.h"
#include "util/files.h"
#include "log.h"

#include <vector>

namespace engine {

Texture::Texture(Renderer* renderer, const uint8_t* bitmap, int width, int height, Filtering filtering) : gfx_(renderer->GetDevice())
{
    gfx::SamplerInfo samplerInfo{};

    samplerInfo.magnify = gfx::Filter::kLinear;

    switch (filtering) {
        case Filtering::kOff:
            samplerInfo.minify = gfx::Filter::kNearest;
            samplerInfo.mipmap = gfx::Filter::kNearest;
            samplerInfo.anisotropic_filtering = false;
            break;
        case Filtering::kBilinear:
            samplerInfo.minify = gfx::Filter::kLinear;
            samplerInfo.mipmap = gfx::Filter::kNearest;
            samplerInfo.anisotropic_filtering = false;
            break;
        case Filtering::kTrilinear:
            samplerInfo.minify = gfx::Filter::kLinear;
            samplerInfo.mipmap = gfx::Filter::kLinear;
            samplerInfo.anisotropic_filtering = false;
            break;
        case Filtering::kAnisotropic:
            samplerInfo.minify = gfx::Filter::kLinear;
            samplerInfo.mipmap = gfx::Filter::kLinear;
            samplerInfo.anisotropic_filtering = true;
    }

    if (renderer->samplers.contains(samplerInfo) == false) {
        renderer->samplers.insert(std::make_pair(samplerInfo, gfx_->CreateSampler(samplerInfo)));
    }

    image_ = gfx_->CreateImage(width, height, bitmap);
    sampler_ = renderer->samplers.at(samplerInfo);

    LOG_DEBUG("Created texture: width: {}, height: {}", width, height);
}

Texture::~Texture()
{
    LOG_DEBUG("Destroying texture...");
    gfx_->DestroyImage(image_);
}

std::unique_ptr<Texture> LoadTextureFromFile(const std::string& path, Texture::Filtering filtering, Renderer* renderer)
{
    int width, height;
    auto bitmap = util::ReadImageFile(path, width, height);
    return std::make_unique<Texture>(renderer, bitmap->data(), width, height, filtering);
}

} // namespace engine