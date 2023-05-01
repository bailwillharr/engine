#ifndef ENGINE_INCLUDE_SYSTEMS_RENDER_H_
#define ENGINE_INCLUDE_SYSTEMS_RENDER_H_

#include "components/renderable.hpp"
#include "components/transform.hpp"
#include "ecs_system.hpp"
#include "gfx.hpp"
#include "gfx_device.hpp"
#include "log.hpp"
#include "scene.hpp"

namespace engine {

class RenderSystem : public System {
 public:
  RenderSystem(Scene* scene);
  ~RenderSystem();

  void OnUpdate(float ts) override;

  void SetCameraEntity(uint32_t entity);

 private:
  GFXDevice* const gfx_;

  struct {
    // only uses transform component, which is required for all entities anyway
    uint32_t cam_entity = 0;
    float vertical_fov_degrees = 70.0f;
    float clip_near = 0.5f;
    float clip_far = 10000.0f;
  } camera_;

  float viewport_aspect_ratio_ = 1.0f;
};

}  // namespace engine

#endif