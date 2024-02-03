#ifndef ENGINE_INCLUDE_RESOURCES_TEXTURE_H_
#define ENGINE_INCLUDE_RESOURCES_TEXTURE_H_

#include <string>

#include "application.h"
#include "gfx_device.h"

namespace engine {

class Texture {
   public:
    
    Texture(Renderer* renderer, const uint8_t* bitmap, int width, int height, gfx::SamplerInfo samplerInfo, bool srgb);

    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    const gfx::Image* GetImage() { return image_; }
    const gfx::Sampler* GetSampler() { return sampler_; }

   private:
    GFXDevice* gfx_;
    const gfx::Image* image_;
    const gfx::Sampler* sampler_; // not owned by Texture, owned by Renderer
};

std::unique_ptr<Texture> LoadTextureFromFile(const std::string& path, gfx::SamplerInfo samplerInfo, Renderer* renderer, bool srgb = true);

} // namespace engine

#endif