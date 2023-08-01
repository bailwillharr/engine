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
    bool& entity_initialised = entity_is_initialised_.at(entity);
    if (entity_initialised == false) {
      if (c->onInit == nullptr) throw std::runtime_error("CustomComponent::onInit not set! Entity: " + std::to_string(entity));
      if (c->onUpdate == nullptr) throw std::runtime_error("CustomComponent::onUpdate not set! Entity: " + std::to_string(entity));
      c->onInit();
      entity_initialised = true;
    }
    c->onUpdate(ts);
  }
}

void CustomBehaviourSystem::OnComponentInsert(uint32_t entity)
{
  entity_is_initialised_.emplace(entity, false);
}

}  // namespace engine