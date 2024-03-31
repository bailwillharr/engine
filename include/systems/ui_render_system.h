#pragma once

#include <glm/mat4x4.hpp>

#include "ecs.h"
#include "scene.h"
#include "gfx.h"

namespace engine {

class UIRenderSystem : public System {
   public:
    UIRenderSystem(Scene* scene);
    ~UIRenderSystem();

    void OnComponentInsert(Entity entity) override;
    void OnUpdate(float ts) override;

   private:
};

} // namespace engine
