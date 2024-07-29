#pragma once

#include <unordered_map>

#include "ecs.h"

/* This system allows for one-off custom components that execute arbitrary code
 * It is similar to Unity's 'MonoBehavior' system */

namespace engine {

class CustomBehaviourSystem : public System {
   public:
    CustomBehaviourSystem(Scene* scene);
    ~CustomBehaviourSystem();

    void onUpdate(float ts) override;
    void onComponentInsert(Entity entity) override;

   private:
    std::unordered_map<Entity, bool> entity_is_initialised_{};
};

} // namespace engine