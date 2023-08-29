#include "resources/texture.h"

#include "application.h"
#include "util/files.h"
#include "log.h"

#include <vector>

namespace engine::resources {

Texture::Texture(Renderer* renderer, const std::string& path,
                 Filtering filtering)
    : gfx_(renderer->GetDevice()) {
  int width, height;
  std::unique_ptr<std::vector<uint8_t>> texbuf =
      util::ReadImageFile(path, &width, &height);

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
    renderer->samplers.insert(
        std::make_pair(samplerInfo, gfx_->CreateSampler(samplerInfo)));
  }

  image_ = gfx_->CreateImage(width, height, texbuf->data());
  descriptor_set_ =
      gfx_->AllocateDescriptorSet(renderer->GetMaterialSetLayout());
  gfx_->UpdateDescriptorCombinedImageSampler(
      descriptor_set_, 0, image_, renderer->samplers.at(samplerInfo));

  LOG_DEBUG("Created texture: {}, width: {} height: {}", path, width, height);
}

Texture::Texture(Renderer* renderer, const uint8_t* bitmap, int width,
                 int height, Filtering filtering)
    : gfx_(renderer->GetDevice()) {
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
    renderer->samplers.insert(
        std::make_pair(samplerInfo, gfx_->CreateSampler(samplerInfo)));
  }

  image_ = gfx_->CreateImage(width, height, bitmap);
  descriptor_set_ =
      gfx_->AllocateDescriptorSet(renderer->GetMaterialSetLayout());
  gfx_->UpdateDescriptorCombinedImageSampler(
      descriptor_set_, 0, image_, renderer->samplers.at(samplerInfo));

  LOG_DEBUG("Created texture: BITMAP, width: {} height: {}", width, height);
}

Texture::~Texture() { 
  LOG_DEBUG("Destroying texture...");
  gfx_->FreeDescriptorSet(descriptor_set_);
  gfx_->DestroyImage(image_);
}

}  // namespace engine::resources
