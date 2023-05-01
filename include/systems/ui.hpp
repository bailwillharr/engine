#ifndef ENGINE_INCLUDE_SYSTEMS_UI_H_
#define ENGINE_INCLUDE_SYSTEMS_UI_H_

#include "ecs_system.hpp"

namespace engine {

class UISystem : public System {
 public:
  UISystem(Scene* scene);

  void OnUpdate(float ts) override;

 private:
};

}  // namespace engine

#endif