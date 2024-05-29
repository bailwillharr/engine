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
    graphics_settings.present_mode = engine::gfx::PresentMode::kTripleBuffered;
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

        //engine::Entity temple = engine::util::LoadGLTF(*start_scene, "C:/games/temple.glb", true);

        start_scene->RegisterComponent<CameraControllerComponent>();
        start_scene->RegisterSystemAtIndex<CameraControllerSystem>(0);
        start_scene->AddComponent<CameraControllerComponent>(camera)->noclip = true;
        start_scene->GetPosition(camera).z += 10.0f;
    }

    engine::Scene* main_scene = app.scene_manager()->CreateEmptyScene();
    {
        /* create camera */
        engine::Entity camera = main_scene->CreateEntity("camera");

        engine::Entity camera_child = main_scene->CreateEntity("camera_child", camera, glm::vec3{0.0f, 0.0f, -3.0f});
        main_scene->GetTransform(camera_child)->is_static = false;
        auto camren = main_scene->AddComponent<engine::MeshRenderableComponent>(camera_child);
        camren->visible = false;
        camren->mesh = GenSphereMesh(app.renderer()->GetDevice(), 1.0f, /*16*/32);
        camren->material = app.GetResource<engine::Material>("builtin.default");

        /* as of right now, the entity with tag 'camera' is used to build the view
         * matrix */

        auto camera_transform = main_scene->GetComponent<engine::TransformComponent>(camera);
        camera_transform->position = {0.0f, 0.0f, 100.0f};
        camera_transform->is_static = false;

        main_scene->RegisterComponent<CameraControllerComponent>();
        main_scene->RegisterSystemAtIndex<CameraControllerSystem>(0);
        main_scene->AddComponent<CameraControllerComponent>(camera);

        /* floor */
        engine::Entity floor = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/floor2.glb"), true);
        //main_scene->GetComponent<engine::MeshRenderableComponent>(main_scene->GetEntity("Cube", floor))->visible = false;

        engine::Entity monke = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/monke.glb"), true);
        main_scene->GetComponent<engine::TransformComponent>(monke)->position.y += 10.0f;

        // engine::Entity bottle = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/bottle.glb"));
        // main_scene->GetComponent<engine::TransformComponent>(bottle)->scale *= 10.0f;
        // main_scene->GetComponent<engine::TransformComponent>(bottle)->position.x += 25.0f;
        // main_scene->GetComponent<engine::TransformComponent>(bottle)->position.z += 5.0f;

        engine::Entity helmet = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/DamagedHelmet.glb"), true);
        main_scene->GetPosition(helmet) += glm::vec3{5.0f, 5.0f, 5.0f};
        main_scene->GetScale(helmet) *= 3.0f;
        main_scene->GetRotation(helmet) = glm::angleAxis(glm::pi<float>(), glm::vec3{ 0.0f, 0.0f, 1.0f });
        main_scene->GetRotation(helmet) *= glm::angleAxis(glm::half_pi<float>(), glm::vec3{ 1.0f, 0.0f, 0.0f });

        engine::Entity toycar = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/ToyCar.glb"), true);
        main_scene->GetScale(toycar) *= 150.0f;
        main_scene->GetPosition(toycar).z -= 0.07f;

        engine::Entity stairs = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/stairs.glb"), true);
        main_scene->GetPosition(stairs) += glm::vec3{-8.0f, -5.0f, 0.1f};
        main_scene->GetRotation(stairs) = glm::angleAxis(glm::half_pi<float>(), glm::vec3{0.0f, 0.0f, 1.0f});
        main_scene->GetRotation(stairs) *= glm::angleAxis(glm::half_pi<float>(), glm::vec3{1.0f, 0.0f, 0.0f});

        engine::Entity axes = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/MY_AXES.glb"), true);
        main_scene->GetPosition(axes) += glm::vec3{-40.0f, -40.0f, 1.0f};

        engine::Entity bottle = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/bottle.glb"), true);
        main_scene->GetPosition(bottle).y -= 10.0f;
        main_scene->GetPosition(bottle).z += 2.5f;
        main_scene->GetScale(bottle) *= 25.0f;

        //engine::Entity cube = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/cube.glb"), false);
        engine::Entity cube = main_scene->CreateEntity("cube", 0, glm::vec3{ 4.0f, -17.0f, 0.0f });
        main_scene->GetTransform(cube)->is_static = false;
        auto cube_ren = main_scene->AddComponent<engine::MeshRenderableComponent>(cube);
        cube_ren->material = app.GetResource<engine::Material>("builtin.default");
        cube_ren->mesh = GenCuboidMesh(app.renderer()->GetDevice(), 1.0f, 1.0f, 1.0f);
        cube_ren->visible = true;
        auto cubeCustom = main_scene->AddComponent<engine::CustomComponent>(cube);
        cubeCustom->onInit = [] {};
        cubeCustom->onUpdate = [&main_scene, cube](float dt) {
            static float yaw = 0.0f;
            yaw += dt;
            main_scene->GetRotation(cube) = glm::angleAxis(yaw, glm::vec3{0.0f, 0.0f, 1.0f});
            main_scene->GetRotation(cube) *= glm::angleAxis(glm::half_pi<float>(), glm::vec3{ 1.0f, 0.0f, 0.0f });
        };


        engine::Entity teapot = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/teapot.glb"), true);
        main_scene->GetPosition(teapot).y += 5.0f;
        main_scene->GetPosition(teapot).x -= 5.0f;
        main_scene->GetScale(teapot) *= 5.0f;

        engine::Entity tree = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/tree.glb"), true);
        main_scene->GetPosition(tree) = glm::vec3{-5.0f, -5.0f, 0.0f};

        engine::Entity box = engine::util::LoadGLTF(*main_scene, app.GetResourcePath("models/box.glb"), true);
        main_scene->GetPosition(box) = glm::vec3{ -5.0f, -17.0f, 0.1f };
        main_scene->GetScale(box) *= 10.0f;
        main_scene->GetRotation(box) = glm::angleAxis(glm::pi<float>() * 0.0f, glm::vec3{ 0.0f, 0.0f, 1.0f });
        main_scene->GetRotation(box) *= glm::angleAxis(glm::half_pi<float>(), glm::vec3{ 1.0f, 0.0f, 0.0f });
    }

    start_scene->GetSystem<CameraControllerSystem>()->next_scene_ = main_scene;
    main_scene->GetSystem<CameraControllerSystem>()->next_scene_ = start_scene;

    app.scene_manager()->SetActiveScene(main_scene);
    app.GameLoop();
}
