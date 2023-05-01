#ifndef ENGINE_INCLUDE_RESOURCES_TEXTURE_H_
#define ENGINE_INCLUDE_RESOURCES_TEXTURE_H_

#include <string>

#include "application.hpp"
#include "gfx_device.hpp"

namespace engine {
namespace resources {

class Texture {
 public:
  enum class Filtering {
    kOff,
    kBilinear,
    kTrilinear,
    kAnisotropic,
  };

  Texture(RenderData* render_data, const std::string& path,
          Filtering filtering);
  ~Texture();
  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  const gfx::Image* GetImage() { return image_; }
  const gfx::DescriptorSet* GetDescriptorSet() { return descriptor_set_; }

 private:
  GFXDevice* gfx_;
  const gfx::Image* image_;
  const gfx::DescriptorSet* descriptor_set_;
};

}  // namespace resources
}  // namespace engine

#endif