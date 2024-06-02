#pragma once

#include <memory>

#include "resource_shader.h"
#include "resource_texture.h"

// a material is just a shader with assigned textures/parameters

namespace engine {

// copyable
class Material {
    const std::shared_ptr<Shader> m_shader;
    std::shared_ptr<Texture> m_texture_albedo;
    std::shared_ptr<Texture> m_texture_normal;
    std::shared_ptr<Texture> m_texture_occlusion_roughness_metallic;
    const gfx::DescriptorSet* m_material_set = nullptr;
    Renderer* const m_renderer;

public:
    Material(Renderer* renderer, std::shared_ptr<engine::Shader> shader);
    Material(const Material&) = delete;

    ~Material();

    Material& operator=(const Material&) = delete;

    void setAlbedoTexture(std::shared_ptr<Texture> texture);
    void setNormalTexture(std::shared_ptr<Texture> texture);
    void setOcclusionRoughnessMetallicTexture(std::shared_ptr<Texture> texture);
    const gfx::DescriptorSet* getDescriptorSet() { return m_material_set; }
    Shader* getShader() { return m_shader.get(); }
};

} // namespace engine