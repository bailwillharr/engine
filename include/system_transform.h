#pragma once

#include "ecs.h"

namespace engine {

class TransformSystem : public System {

   public:
    TransformSystem(Scene* scene);

    void onUpdate(float ts) override;

    /*
     * Linear-searches for an entity that matches the arguments' criteria.
     * Take care not to create multiple entities under a parent with the same tag, as this search will only find the first in the list.
     */
    Entity GetChildEntity(Entity parent, const std::string& tag);
};

} // namespace engine