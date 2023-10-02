#include "game.hpp"

#include "application.h"
#include "camera_controller.hpp"
#include "components/collider.h"
#include "components/custom.h"
#include "components/mesh_renderable.h"
#include "components/transform.h"
#include "input_manager.h"
#include "meshgen.hpp"
#include "resources/font.h"
#include "resources/material.h"
#include "resources/texture.h"
#include "scene.h"
#include "scene_manager.h"
#include "systems/mesh_render_system.h"
#include "systems/transform.h"
#include "util/gltf_loader.h"
#include "util/model_loader.h"
#include "log.h"
#include "window.h"

#include "config.h"

static void ConfigureInputs(engine::InputManager* input_manager)
{
    // user interface mappings
    input_manager->AddInputButton("fullscreen", engine::inputs::Key::K_F11);
    input_manager->AddInputButton("exit", engine::inputs::Key::K_ESCAPE);
    // game buttons
    input_manager->AddInputButton("fire", engine::inputs::MouseButton::M_LEFT);
    input_manager->AddInputButton("aim", engine::inputs::MouseButton::M_RIGHT);
    input_manager->AddInputButton("jump", engine::inputs::Key::K_SPACE);
    input_manager->AddInputButton("sprint", engine::inputs::Key::K_LSHIFT);
    // game movement
    input_manager->AddInputButtonAsAxis("movex", engine::inputs::Key::K_D, engine::inputs::Key::K_A);
    input_manager->AddInputButtonAsAxis("movey", engine::inputs::Key::K_W, engine::inputs::Key::K_S);
    // looking around
    input_manager->AddInputAxis("lookx", engine::inputs::MouseAxis::X);
    input_manager->AddInputAxis("looky", engine::inputs::MouseAxis::Y);
}

void PlayGame(GameSettings settings)
{
    LOG_INFO("FPS limiter: {}", settings.enable_frame_limiter ? "ON" : "OFF");
    LOG_INFO("Graphics Validation: {}", settings.enable_validation ? "ON" : "OFF");

    engine::gfx::GraphicsSettings graphics_settings{};
    graphics_settings.enable_validation = settings.enable_validation;
    graphics_settings.vsync = true;
    graphics_settings.wait_for_present = false;
    graphics_settings.msaa_level = engine::gfx::MSAALevel::kOff;

    engine::Application app(PROJECT_NAME, PROJECT_VERSION, graphics_settings);
    app.SetFrameLimiter(settings.enable_frame_limiter);
    app.window()->SetRelativeMouseMode(true);
    ConfigureInputs(app.input_manager());

    {
        static auto my_scene = app.scene_manager()->CreateEmptyScene();

        /* create camera */
        {
            auto camera = my_scene->CreateEntity("camera");

            /* as of right now, the entity with tag 'camera' is used to build the view
             * matrix */

            auto camera_transform = my_scene->GetComponent<engine::TransformComponent>(camera);
            camera_transform->position = {0.0f, 0.0f, 10.0f};

            my_scene->RegisterComponent<CameraControllerComponent>();
            my_scene->RegisterSystem<CameraControllerSystem>();
            my_scene->AddComponent<CameraControllerComponent>(camera);
        }

        /* shared resources */
        auto grass_texture = std::make_shared<engine::resources::Texture>(app.renderer(), app.GetResourcePath("textures/grass.png"),
                                                                          engine::resources::Texture::Filtering::kAnisotropic);

        auto sky_texture = std::make_shared<engine::resources::Texture>(app.renderer(), app.GetResourcePath("textures/sky.jpg"),
                                                                        engine::resources::Texture::Filtering::kAnisotropic);

        /* skybox */
        {
            engine::Entity skybox = my_scene->CreateEntity("skybox");

            auto skybox_renderable = my_scene->AddComponent<engine::MeshRenderableComponent>(skybox);
            skybox_renderable->material = std::make_unique<engine::resources::Material>(app.GetResource<engine::resources::Shader>("builtin.skybox"));
            skybox_renderable->material->texture_ = sky_texture;
            skybox_renderable->mesh = GenCuboidMesh(app.renderer()->GetDevice(), 10.0f, 10.0f, 10.0f, 1.0f, true);

            auto skybox_transform = my_scene->GetComponent<engine::TransformComponent>(skybox);
            skybox_transform->is_static = true;
            skybox_transform->position = {-5.0f, -5.0f, -5.0f};
        }

        /* floor */
        {
            engine::Entity floor = engine::util::LoadMeshFromFile(my_scene, app.GetResourcePath("models/terrain.dae"), true);

            auto floor_transform = my_scene->GetComponent<engine::TransformComponent>(floor);
            floor_transform->is_static = true;
        }

        /* building */
        {
            auto cobbleHouse = engine::util::LoadMeshFromFile(my_scene, app.GetResourcePath("models/cobble_house/cobble_house.dae"), false);
            my_scene->GetComponent<engine::TransformComponent>(cobbleHouse)->position += glm::vec3{33.0f, 35.0f, 0.1f};
            auto cobbleCustom = my_scene->AddComponent<engine::CustomComponent>(cobbleHouse);
            cobbleCustom->onInit = [](void) { LOG_INFO("Cobble house spin component initialised!"); };
            cobbleCustom->onUpdate = [&](float ts) {
                static auto t = my_scene->GetComponent<engine::TransformComponent>(cobbleHouse);
                t->rotation *= glm::angleAxis(ts, glm::vec3{0.0f, 0.0f, 1.0f});
            };
        }

        {
            engine::Entity cube = my_scene->CreateEntity("cube", 0, glm::vec3{40.0f, 10.0f, 5.0f});
            auto cubeRenderable = my_scene->AddComponent<engine::MeshRenderableComponent>(cube);
            cubeRenderable->mesh = GenCuboidMesh(app.renderer()->GetDevice(), 1.0f, 1.0f, 1.0f);
            cubeRenderable->material = std::make_unique<engine::resources::Material>(app.GetResource<engine::resources::Shader>("builtin.standard"));
            cubeRenderable->material->texture_ = std::make_unique<engine::resources::Texture>(app.renderer(), app.GetResourcePath("textures/uvcheck.png"),
                                                                                              engine::resources::Texture::Filtering::kAnisotropic);
        }

        /* some text */
        {
            engine::Entity textbox = my_scene->CreateEntity("textbox", 0, glm::vec3{0.0f, 0.8f, 0.0f});
            auto textboxComponent = my_scene->AddComponent<engine::CustomComponent>(textbox);
            textboxComponent->onInit = [](void) { LOG_DEBUG("Textbox custom component initialised!"); };

            textboxComponent->onUpdate = [&](float ts) {
                static float time_elapsed;
                time_elapsed += ts;
                if (time_elapsed >= 1.0f) {
                    time_elapsed = 0.0f;
                }
            };
        }

        engine::util::LoadMeshFromFile(my_scene, app.GetResourcePath("models/MY_AXES.dae"), true);
        my_scene->GetComponent<engine::TransformComponent>(engine::util::LoadMeshFromFile(my_scene, app.GetResourcePath("models/uvcheck.dae"), true))
            ->position += glm::vec3{20.0f, 20.0f, 20.0f};

        /* teapot */
        my_scene->GetComponent<engine::TransformComponent>(engine::util::LoadMeshFromFile(my_scene, app.GetResourcePath("models/teapot.dae"), true))
            ->position += glm::vec3{10.0f, 10.0f, 10.0f};

        //engine::util::LoadGLTF(*my_scene, app.GetResourcePath("engine/models/test/test.gltf"));

        app.GameLoop();
    }
}
