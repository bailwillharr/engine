#include "system_custom_behaviour.h"

#include <typeinfo>

#include "component_custom.h"
#include "component_transform.h"
#include "scene.h"

namespace engine {

CustomBehaviourSystem::CustomBehaviourSystem(Scene* scene)
    : System(scene, {typeid(TransformComponent).hash_code(),
                     typeid(CustomComponent).hash_code()}) {
  // constructor here
}

CustomBehaviourSystem::~CustomBehaviourSystem() {}

void CustomBehaviourSystem::onUpdate(float ts) {
  for (Entity entity : m_entities) {
    auto c = m_scene->GetComponent<CustomComponent>(entity);
    assert(c != nullptr);
    bool& entity_initialised = entity_is_initialised_.at(entity);
    if (entity_initialised == false) {
      if (c->on_init == nullptr) throw std::runtime_error("CustomComponent::on_init not set! Entity: " + std::to_string(entity));
      if (c->on_update == nullptr) throw std::runtime_error("CustomComponent::on_update not set! Entity: " + std::to_string(entity));
      c->on_init();
      entity_initialised = true;
    }
    c->on_update(ts);
  }
}

void CustomBehaviourSystem::onComponentInsert(Entity entity)
{
  entity_is_initialised_.emplace(entity, false);
}

}  // namespace engine