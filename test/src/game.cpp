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

static void ConfigureInputs(engine::InputManager& input_manager)
{
    // user interface mappings
    input_manager.AddInputButton("fullscreen", engine::inputs::Key::K_F11);
    input_manager.AddInputButton("exit", engine::inputs::Key::K_ESCAPE);
    // game buttons
    input_manager.AddInputButton("fire", engine::inputs::MouseButton::M_LEFT);
    input_manager.AddInputButton("aim", engine::inputs::MouseButton::M_RIGHT);
    input_manager.AddInputButton("jump", engine::inputs::Key::K_SPACE);
    input_manager.AddInputButton("sprint", engine::inputs::Key::K_LSHIFT);
    // game movement
    input_manager.AddInputButtonAsAxis("movex", engine::inputs::Key::K_D, engine::inputs::Key::K_A);
    input_manager.AddInputButtonAsAxis("movey", engine::inputs::Key::K_W, engine::inputs::Key::K_S);
    // looking around
    input_manager.AddInputAxis("lookx", engine::inputs::MouseAxis::X);
    input_manager.AddInputAxis("looky", engine::inputs::MouseAxis::Y);
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
    graphics_settings.enable_anisotropy = true;

    engine::Application::Configuration configuration{};
    configuration.enable_frame_limiter = settings.enable_frame_limiter;

    engine::Application app(PROJECT_NAME, PROJECT_VERSION, graphics_settings, configuration);
    app.window()->SetRelativeMouseMode(true);
    ConfigureInputs(*app.input_manager());

    engine::Scene* start_scene = app.scene_manager()->CreateEmptyScene();
    {
        /* create camera */
        engine::Entity camera = start_scene->CreateEntity("camera");

        /* as of right now, the entity with tag 'camera' is used to build the view
         * matrix */

        start_scene->RegisterComponent<CameraControllerComponent>();
        start_scene->RegisterSystem<CameraControllerSystem>();
        start_scene->AddComponent<CameraControllerComponent>(camera);
    }

    engine::Scene* main_scene = app.scene_manager()->CreateEmptyScene();
    {
        /* create camera */
        engine::Entity camera = main_scene->CreateEntity("camera");

        /* as of right now, the entity with tag 'camera' is used to build the view
         * matrix */

        auto camera_transform = main_scene->GetComponent<engine::TransformComponent>(camera);
        camera_transform->position = {-5.0f, -10.0f, 4.0f};

        main_scene->RegisterComponent<CameraControllerComponent>();
        main_scene->RegisterSystem<CameraControllerSystem>();
        main_scene->AddComponent<CameraControllerComponent>(camera);

        /* floor */
        engine::Entity floor = main_scene->CreateEntity("floor", 0, glm::vec3{-50.0f, -50.0f, 0.0f});
        auto floor_renderable = main_scene->AddComponent<engine::MeshRenderableComponent>(floor);
        floor_renderable->mesh = GenCuboidMesh(app.renderer()->GetDevice(), 100.0f, 100.0f, 0.1f, 50.0f);
        floor_renderable->material = std::make_unique<engine::Material>(app.renderer(), app.GetResource<engine::Shader>("builtin.fancy"));
        std::shared_ptr<engine::Texture> floor_albedo =
            engine::LoadTextureFromFile(app.GetResourcePath("textures/bricks-mortar-albedo.png"), engine::gfx::SamplerInfo{}, app.renderer());
        std::shared_ptr<engine::Texture> floor_normal =
            engine::LoadTextureFromFile(app.GetResourcePath("textures/bricks-mortar-normal.png"), engine::gfx::SamplerInfo{}, app.renderer(), false);
        std::shared_ptr<engine::Texture> floor_mr =
            engine::LoadTextureFromFile(app.GetResourcePath("textures/bricks-mortar-roughness.png"), engine::gfx::SamplerInfo{}, app.renderer(), false);
        floor_renderable->material->SetAlbedoTexture(floor_albedo);
        floor_renderable->material->SetNormalTexture(floor_normal);
        floor_renderable->material->SetMetallicRoughnessTexture(floor_mr);
        floor_renderable->material->SetOcclusionTexture(app.GetResource<engine::Texture>("builtin.white"));
        floor_renderable->visible = true ;
        auto floor_col = main_scene->AddComponent<engine::ColliderComponent>(floor);
        floor_col->aabb.min = glm::vec3{0.0f, 0.0f, 0.0f};
        floor_col->aabb.max = glm::vec3{100.0f, 100.0f, 0.1f };


        engine::Entity monke = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/monke.glb"));
        main_scene->GetComponent<engine::TransformComponent>(monke)->position.y += 10.0f;

        //engine::Entity bottle = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/bottle.glb"));
        //main_scene->GetComponent<engine::TransformComponent>(bottle)->scale *= 10.0f;
        //main_scene->GetComponent<engine::TransformComponent>(bottle)->position.x += 25.0f;
        //main_scene->GetComponent<engine::TransformComponent>(bottle)->position.z += 5.0f;

        /* skybox */
        engine::Entity skybox = main_scene->CreateEntity("skybox");
        auto skybox_transform = main_scene->GetComponent<engine::TransformComponent>(skybox);
        skybox_transform->is_static = true;
        skybox_transform->position = { -5.0f, -5.0f, -5.0f };
        auto skybox_renderable = main_scene->AddComponent<engine::MeshRenderableComponent>(skybox);
        skybox_renderable->mesh = GenCuboidMesh(app.renderer()->GetDevice(), 10.0f, 10.0f, 10.0f, 1.0f, true);
        skybox_renderable->material = std::make_unique<engine::Material>(app.renderer(), app.GetResource<engine::Shader>("builtin.skybox"));
        skybox_renderable->material->SetAlbedoTexture(app.GetResource<engine::Texture>("builtin.black"));

		engine::Entity helmet = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/DamagedHelmet.glb"));
		main_scene->GetPosition(helmet) += glm::vec3{20.0f, 10.0f, 5.0f};

		engine::Entity toycar = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/ToyCar.glb"));
		main_scene->GetScale(toycar) *= 100.0f;
		auto car_spin = main_scene->AddComponent<engine::CustomComponent>(toycar);
		car_spin->onInit = []() -> void {};
		car_spin->onUpdate = [&](float dt) -> void {
			static float yaw = 0.0f;
			yaw += dt;
			main_scene->GetRotation(toycar) = glm::angleAxis(yaw, glm::vec3{0.0f, 0.0f, 1.0f});
			main_scene->GetRotation(toycar) *= glm::angleAxis(glm::half_pi<float>(), glm::vec3{1.0f, 0.0f, 0.0f});
		};
    }

    start_scene->GetSystem<CameraControllerSystem>()->next_scene_ = main_scene;
    main_scene->GetSystem<CameraControllerSystem>()->next_scene_ = start_scene;

    app.scene_manager()->SetActiveScene(start_scene);
    app.GameLoop();
}
