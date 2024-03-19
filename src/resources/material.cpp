#include "resources/material.h"

#include "resources/shader.h"

namespace engine {

Material::Material(Renderer* renderer, std::shared_ptr<Shader> shader) : shader_(shader), renderer_(renderer)
{
    material_set_ = renderer->GetDevice()->AllocateDescriptorSet(renderer->GetMaterialSetLayout());
}

Material::~Material() { renderer_->GetDevice()->FreeDescriptorSet(material_set_); }

void Material::SetAlbedoTexture(std::shared_ptr<Texture> texture)
{
    renderer_->GetDevice()->UpdateDescriptorCombinedImageSampler(material_set_, 0, texture->GetImage(), texture->GetSampler());
    texture_albedo_ = texture;
}

void Material::SetNormalTexture(std::shared_ptr<Texture> texture)
{
    renderer_->GetDevice()->UpdateDescriptorCombinedImageSampler(material_set_, 1, texture->GetImage(), texture->GetSampler());
    texture_normal_ = texture;
}

void Material::SetOcclusionRoughnessMetallicTexture(std::shared_ptr<Texture> texture)
{
    renderer_->GetDevice()->UpdateDescriptorCombinedImageSampler(material_set_, 2, texture->GetImage(), texture->GetSampler());
    texture_occlusion_roughness_metallic_ = texture;
}

} // namespace engine
