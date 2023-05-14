#include "resources/texture.h"

#include "application.h"
#include "util/files.h"
#include "log.h"

#include <vector>

namespace engine::resources {

Texture::Texture(RenderData* renderData, const std::string& path,
                 Filtering filtering)
    : gfx_(renderData->gfxdev.get()) {
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

  if (renderData->samplers.contains(samplerInfo) == false) {
    renderData->samplers.insert(
        std::make_pair(samplerInfo, gfx_->CreateSampler(samplerInfo)));
  }

  image_ = gfx_->CreateImage(width, height, texbuf->data());
  descriptor_set_ =
      gfx_->AllocateDescriptorSet(renderData->material_set_layout);
  gfx_->UpdateDescriptorCombinedImageSampler(
      descriptor_set_, 0, image_, renderData->samplers.at(samplerInfo));

  LOG_INFO("Loaded texture: {}, width: {} height: {}", path, width, height);
}

Texture::Texture(RenderData* render_data, const uint8_t* bitmap, int width,
                 int height, Filtering filtering)
    : gfx_(render_data->gfxdev.get()) {
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

  if (render_data->samplers.contains(samplerInfo) == false) {
    render_data->samplers.insert(
        std::make_pair(samplerInfo, gfx_->CreateSampler(samplerInfo)));
  }

  image_ = gfx_->CreateImage(width, height, bitmap);
  descriptor_set_ =
      gfx_->AllocateDescriptorSet(render_data->material_set_layout);
  gfx_->UpdateDescriptorCombinedImageSampler(
      descriptor_set_, 0, image_, render_data->samplers.at(samplerInfo));

  LOG_INFO("Loaded texture: BITMAP, width: {} height: {}", width, height);
}

Texture::~Texture() { gfx_->DestroyImage(image_); }

}  // namespace engine::resources
