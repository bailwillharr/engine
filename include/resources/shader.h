#ifndef ENGINE_INCLUDE_RESOURCES_SHADER_H_
#define ENGINE_INCLUDE_RESOURCES_SHADER_H_

#include "application.h"
#include "gfx.h"
#include "gfx_device.h"

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

  struct ShaderSettings {
    VertexParams vertexParams;
    bool alpha_blending;
    bool cull_backface;
    bool write_z;
    int render_order;
  };

  static constexpr int kHighestRenderOrder = 1;

  Shader(RenderData* render_data, const char* vert_path, const char* frag_path,
         const ShaderSettings& settings);
  ~Shader();
  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;

  const gfx::Pipeline* GetPipeline();
  int GetRenderOrder() { return render_order_; }

 private:
  GFXDevice* const gfx_;
  const gfx::Pipeline* pipeline_;
  const int render_order_;
};

}  // namespace resources
}  // namespace engine

#endif