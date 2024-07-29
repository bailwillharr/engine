#include "system_custom_behaviour.h"

#include <typeinfo>

#include "component_custom.h"
#include "component_transform.h"
#include "scene.h"

namespace engine {

CustomBehaviourSystem::CustomBehaviourSystem(Scene* scene) : System(scene, {typeid(TransformComponent).hash_code(), typeid(CustomComponent).hash_code()})
{
    // constructor here
}

CustomBehaviourSystem::~CustomBehaviourSystem() {}

void CustomBehaviourSystem::onUpdate(float ts)
{
    for (Entity entity : m_entities) {
        auto c = m_scene->GetComponent<CustomComponent>(entity);
        assert(c != nullptr);
        assert(c->impl != nullptr);
        bool& entity_initialised = m_entity_is_initialised.at(entity);
        if (entity_initialised == false) {
            c->impl->m_entity = entity;
            c->impl->m_scene = m_scene;
            c->impl->init();
            entity_initialised = true;
        }
        c->impl->update(ts);
    }
}

void CustomBehaviourSystem::onComponentInsert(Entity entity) { m_entity_is_initialised.emplace(entity, false); }

} // namespace engine