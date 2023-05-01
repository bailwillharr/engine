#ifndef ENGINE_INCLUDE_RESOURCES_MATERIAL_H_
#define ENGINE_INCLUDE_RESOURCES_MATERIAL_H_

#include <memory>

#include "resources/shader.hpp"
#include "resources/texture.hpp"

namespace engine {
namespace resources {

// copyable
class Material {
 public:
  Material(std::shared_ptr<Shader> shader);
  ~Material() = default;
  Material(const Material&);
  Material& operator=(const Material&) = delete;

  auto GetShader() { return shader_.get(); }

  std::shared_ptr<Texture> texture_;

 private:
  const std::shared_ptr<Shader> shader_;
};

}  // namespace resources
}  // namespace engine

#endif