#ifndef ENGINE_INCLUDE_SYSTEMS_RENDER2D_H_
#define ENGINE_INCLUDE_SYSTEMS_RENDER2D_H_

#include "ecs_system.hpp"

namespace engine {

class Render2DSystem : public System {
 public:
  Render2DSystem(Scene* scene);
  ~Render2DSystem();

  void OnUpdate(float ts) override;

 private:
};

}  // namespace engine

#endif