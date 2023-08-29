#include "renderer.h"

namespace engine {

Renderer::Renderer(const char* app_name, const char* app_version,
                   SDL_Window* window, gfx::GraphicsSettings settings) {
  device_ =
      std::make_unique<GFXDevice>(app_name, app_version, window, settings);

  // sort out descriptor set layouts:
  std::vector<gfx::DescriptorSetLayoutBinding> globalSetBindings;
  {
    auto& binding0 = globalSetBindings.emplace_back();
    binding0.descriptor_type = gfx::DescriptorType::kUniformBuffer;
    binding0.stage_flags = gfx::ShaderStageFlags::kVertex;
  }
  global_uniform.layout = device_->CreateDescriptorSetLayout(globalSetBindings);
  global_uniform.set = device_->AllocateDescriptorSet(global_uniform.layout);
  global_uniform.uniform_buffer_data.data = glm::mat4{1.0f};
  global_uniform.uniform_buffer =
      device_->CreateUniformBuffer(sizeof(global_uniform.uniform_buffer_data),
                                   &global_uniform.uniform_buffer_data);
  device_->UpdateDescriptorUniformBuffer(
      global_uniform.set, 0, global_uniform.uniform_buffer, 0,
      sizeof(global_uniform.uniform_buffer_data));

  std::vector<gfx::DescriptorSetLayoutBinding> frameSetBindings;
  {
    auto& binding0 = frameSetBindings.emplace_back();
    binding0.descriptor_type = gfx::DescriptorType::kUniformBuffer;
    binding0.stage_flags = gfx::ShaderStageFlags::kVertex;
  }
  frame_uniform.layout = device_->CreateDescriptorSetLayout(frameSetBindings);
  frame_uniform.set = device_->AllocateDescriptorSet(frame_uniform.layout);
  frame_uniform.uniform_buffer_data.data = glm::mat4{1.0f};
  frame_uniform.uniform_buffer =
      device_->CreateUniformBuffer(sizeof(frame_uniform.uniform_buffer_data),
                                   &frame_uniform.uniform_buffer_data);
  device_->UpdateDescriptorUniformBuffer(
      frame_uniform.set, 0, frame_uniform.uniform_buffer, 0,
      sizeof(frame_uniform.uniform_buffer_data));

  std::vector<gfx::DescriptorSetLayoutBinding> materialSetBindings;
  {
    auto& binding0 = materialSetBindings.emplace_back();
    binding0.descriptor_type = gfx::DescriptorType::kCombinedImageSampler;
    binding0.stage_flags = gfx::ShaderStageFlags::kFragment;
  }
  material_set_layout = device_->CreateDescriptorSetLayout(materialSetBindings);
};

Renderer::~Renderer() {
  for (const auto& [info, sampler] : samplers) {
    device_->DestroySampler(sampler);
  }
  device_->DestroyDescriptorSetLayout(material_set_layout);

  device_->DestroyUniformBuffer(frame_uniform.uniform_buffer);
  device_->DestroyDescriptorSetLayout(frame_uniform.layout);

  device_->DestroyUniformBuffer(global_uniform.uniform_buffer);
  device_->DestroyDescriptorSetLayout(global_uniform.layout);
}

}  // namespace engine