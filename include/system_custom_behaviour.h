#pragma once

#include <unordered_map>

#include "ecs.h"

/* This system allows for one-off custom components that execute arbitrary code
 * It is similar to Unity's 'MonoBehavior' system */

namespace engine {

class CustomBehaviourSystem : public System {
private:
    std::unordered_map<Entity, bool> m_entity_is_initialised{};

public:
    CustomBehaviourSystem(Scene* scene);

    ~CustomBehaviourSystem();

    void onUpdate(float ts) override;
    void onComponentInsert(Entity entity) override;
};

} // namespace engine