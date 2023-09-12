#include "systems/ui_render_system.h"

#include "components/transform.h"
#include "components/ui_renderable.h"

namespace engine {

    UIRenderSystem::UIRenderSystem(Scene* scene)
        : System(scene, { typeid(TransformComponent).hash_code(),
                         typeid(UIRenderableComponent).hash_code() }) {}

    UIRenderSystem::~UIRenderSystem() {}

    void UIRenderSystem::OnComponentInsert(uint32_t entity) {
        (void)entity;
    }

    void UIRenderSystem::OnUpdate(float ts) {
        (void)ts;
    }

}  // namespace engine
