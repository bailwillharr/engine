#include "resources/shader.h"

#include "application.h"
#include "gfx_device.h"
#include "log.h"

#include <glm/mat4x4.hpp>

namespace engine {

Shader::Shader(Renderer* renderer, const std::string& vertPath, const std::string& fragPath,
               const ShaderSettings& settings)
    : gfx_(renderer->GetDevice()), render_order_(settings.render_order) {
  assert(settings.render_order <= kHighestRenderOrder &&
         settings.render_order >= 0);
  uint32_t index = 0;
  uint32_t stride = 0;
  gfx::VertexFormat vertFormat{};

  vertFormat.attribute_descriptions.emplace_back(
      index++, gfx::VertexAttribFormat::kFloat3, stride);
  stride += 3 * sizeof(float);

  if (settings.vertexParams.has_normal) {
    vertFormat.attribute_descriptions.emplace_back(
        index++, gfx::VertexAttribFormat::kFloat3, stride);
    stride += 3 * sizeof(float);
  }
  if (settings.vertexParams.has_tangent) {
    vertFormat.attribute_descriptions.emplace_back(
        index++, gfx::VertexAttribFormat::kFloat4, stride);
    stride += 4 * sizeof(float);
  }
  if (settings.vertexParams.has_color) {
    vertFormat.attribute_descriptions.emplace_back(
        index++, gfx::VertexAttribFormat::kFloat4, stride);
    stride += 4 * sizeof(float);
  }
  if (settings.vertexParams.has_uv0) {
    vertFormat.attribute_descriptions.emplace_back(
        index++, gfx::VertexAttribFormat::kFloat2, stride);
    stride += 2 * sizeof(float);
  }
  vertFormat.stride = stride;

  gfx::PipelineInfo info{};
  info.vert_shader_path = vertPath;
  info.frag_shader_path = fragPath;
  info.vertex_format = vertFormat;
  info.alpha_blending = settings.alpha_blending;
  info.face_cull_mode = settings.cull_backface ? gfx::CullMode::kCullBack : gfx::CullMode::kCullNone;
  info.write_z = settings.write_z;
  info.line_primitives = false;
  info.descriptor_set_layouts.push_back(renderer->GetGlobalSetLayout());
  info.descriptor_set_layouts.push_back(renderer->GetFrameSetLayout());
  info.descriptor_set_layouts.push_back(renderer->GetMaterialSetLayout());

  pipeline_ = gfx_->CreatePipeline(info);

  LOG_DEBUG("Created shader: {}, pipeline: {}", vertPath,
           static_cast<const void*>(pipeline_));
}

Shader::~Shader() {
  gfx_->DestroyPipeline(pipeline_);
  LOG_DEBUG("Destroyed shader, pipeline: {}", static_cast<const void*>(pipeline_));
}

const gfx::Pipeline* Shader::GetPipeline() { return pipeline_; }

}  // namespace engine
