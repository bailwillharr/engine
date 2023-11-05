#ifndef ENGINE_INCLUDE_RESOURCES_MATERIAL_H_
#define ENGINE_INCLUDE_RESOURCES_MATERIAL_H_

#include <memory>

#include "resources/shader.h"
#include "resources/texture.h"

// a material is just a shader with assigned textures/parameters

namespace engine {

// copyable
class Material {
   public:
    Material(Renderer* renderer, std::shared_ptr<engine::Shader> shader);
    ~Material();
    Material& operator=(const Material&) = delete;

    void SetAlbedoTexture(std::shared_ptr<Texture> texture);
    void SetNormalTexture(std::shared_ptr<Texture> texture);
    void SetOcclusionTexture(std::shared_ptr<Texture> texture);
    void SetMetallicRoughnessTexture(std::shared_ptr<Texture> texture);

    const gfx::DescriptorSet* GetDescriptorSet() { return material_set_; }
    Shader* GetShader() { return shader_.get(); }

   private:
    const std::shared_ptr<Shader> shader_;
    std::shared_ptr<Texture> texture_albedo_;
    std::shared_ptr<Texture> texture_normal_;
    std::shared_ptr<Texture> texture_occlusion_;
    std::shared_ptr<Texture> texture_metallic_roughness_;

    const gfx::DescriptorSet* material_set_ = nullptr;

    Renderer* const renderer_;
};

} // namespace engine

#endif