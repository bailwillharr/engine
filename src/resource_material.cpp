#include "resource_material.h"

#include "resource_shader.h"

namespace engine {

Material::Material(Renderer* renderer, std::shared_ptr<Shader> shader) : m_shader(shader), m_renderer(renderer)
{
    m_material_set = renderer->GetDevice()->AllocateDescriptorSet(renderer->GetMaterialSetLayout());
    LOG_DEBUG("Created material");
}

Material::~Material() { m_renderer->GetDevice()->FreeDescriptorSet(m_material_set); LOG_DEBUG("Destroyed material"); }

void Material::setAlbedoTexture(std::shared_ptr<Texture> texture)
{
    m_renderer->GetDevice()->UpdateDescriptorCombinedImageSampler(m_material_set, 0, texture->GetImage(), texture->GetSampler());
    m_texture_albedo = texture;
}

void Material::setNormalTexture(std::shared_ptr<Texture> texture)
{
    m_renderer->GetDevice()->UpdateDescriptorCombinedImageSampler(m_material_set, 1, texture->GetImage(), texture->GetSampler());
    m_texture_normal = texture;
}

void Material::setOcclusionRoughnessMetallicTexture(std::shared_ptr<Texture> texture)
{
    m_renderer->GetDevice()->UpdateDescriptorCombinedImageSampler(m_material_set, 2, texture->GetImage(), texture->GetSampler());
    m_texture_occlusion_roughness_metallic = texture;
}

} // namespace engine
