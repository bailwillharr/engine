#ifndef ENGINE_INCLUDE_RESOURCES_SHADER_H_
#define ENGINE_INCLUDE_RESOURCES_SHADER_H_

#include "application.hpp"
#include "gfx.hpp"
#include "gfx_device.hpp"

namespace engine {
namespace resources {

class Shader {
 public:
  // defines what vertex inputs are defined, position is always vec3
  struct VertexParams {
    bool has_normal;   // vec3
    bool has_tangent;  // vec3
    bool has_color;    // vec3
    bool has_uv0;      // vec2
  };

  Shader(RenderData* render_data, const char* vert_path, const char* frag_path,
         const VertexParams& vertex_params, bool alpha_blending,
         bool cull_backface);
  ~Shader();
  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;

  const gfx::Pipeline* GetPipeline();

 private:
  GFXDevice* const gfx_;
  const gfx::Pipeline* pipeline_;
};

}  // namespace resources
}  // namespace engine

#endif