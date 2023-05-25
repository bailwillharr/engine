#include "systems/custom_behaviour.h"

#include <typeinfo>

#include "components/custom.h"
#include "components/transform.h"
#include "scene.h"

namespace engine {

CustomBehaviourSystem::CustomBehaviourSystem(Scene* scene)
    : System(scene, {typeid(TransformComponent).hash_code(),
                     typeid(CustomComponent).hash_code()}) {
  // constructor here
}

CustomBehaviourSystem::~CustomBehaviourSystem() {}

void CustomBehaviourSystem::OnUpdate(float ts) {
  for (uint32_t entity : entities_) {
    auto c = scene_->GetComponent<CustomComponent>(entity);
    assert(c != nullptr);
    c->onUpdate(ts);
  }
}

}  // namespace engine