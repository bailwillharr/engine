#ifndef ENGINE_INCLUDE_CUSTOM_BEHAVIOUR_H_
#define ENGINE_INCLUDE_CUSTOM_BEHAVIOUR_H_

#include <unordered_map>

#include "ecs_system.h"

/* This system allows for one-off custom components that execute arbitrary code
 * It is similar to Unity's 'MonoBehavior' system */

namespace engine {

class CustomBehaviourSystem : public System {
 public:
  CustomBehaviourSystem(Scene* scene);
  ~CustomBehaviourSystem();

  void OnUpdate(float ts) override;
  void OnComponentInsert(uint32_t entity) override;

 private:
  std::unordered_map<uint32_t, bool> entity_is_initialised_{};
};

}  // namespace engine

#endif