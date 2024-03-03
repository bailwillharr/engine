#pragma once

#include "application.h"
#include "gfx.h"
#include "gfx_device.h"

namespace engine {

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

  Shader(Renderer* renderer, const std::string& vert_path, const std::string& frag_path,
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

}  // namespace engine