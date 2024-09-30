#include "game.hpp"

#include "application.h"
#include "camera_controller.hpp"
#include "component_collider.h"
#include "component_custom.h"
#include "component_mesh.h"
#include "component_transform.h"
#include "gltf_loader.h"
#include "input_manager.h"
#include "log.h"
#include "meshgen.hpp"
#include "renderer.h"
#include "resource_font.h"
#include "resource_material.h"
#include "resource_texture.h"
#include "scene.h"
#include "scene_manager.h"
#include "system_mesh_render.h"
#include "system_transform.h"
#include "window.h"

#include "terrain.h"

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
    graphics_settings.present_mode = engine::gfx::PresentMode::TRIPLE_BUFFERED;
    graphics_settings.msaa_level = engine::gfx::MSAALevel::MSAA_OFF;
    graphics_settings.enable_anisotropy = false;

    engine::AppConfiguration configuration{};
    configuration.enable_frame_limiter = settings.enable_frame_limiter;

    engine::Application app(PROJECT_NAME, PROJECT_VERSION, graphics_settings, configuration);
    app.getWindow()->SetRelativeMouseMode(true);
    ConfigureInputs(*app.getInputManager());
    
    engine::Scene* start_scene = app.getSceneManager()->CreateEmptyScene();
    {
        /* create camera */
        engine::Entity camera = start_scene->CreateEntity("camera");

        const engine::Entity temple = engine::loadGLTF(*start_scene, "C:\\Games\\temple.glb", true);

        /* as of right now, the entity with tag 'camera' is used to build the view
         * matrix */

        // engine::Entity temple = engine::loadGLTF(*start_scene, "C:/games/temple.glb", true);

        const engine::Entity sphere = start_scene->CreateEntity("sphere");
        const auto sphere_ren = start_scene->AddComponent<engine::MeshRenderableComponent>(sphere);
        sphere_ren->mesh = GenSphereMesh(app.getRenderer()->GetDevice(), 5.0f, 16);
        sphere_ren->material = app.getResource<engine::Material>("builtin.default");
        sphere_ren->visible = true;
        // start_scene->GetPosition(sphere).z += 0f;

        start_scene->RegisterComponent<CameraControllerComponent>();
        start_scene->RegisterSystemAtIndex<CameraControllerSystem>(0);
        start_scene->AddComponent<CameraControllerComponent>(camera)->noclip = false;
        start_scene->GetPosition(camera).z += 10.0f;
    }

    engine::Scene* const main_scene = app.getSceneManager()->CreateEmptyScene();
    {
        /* create camera */
        const engine::Entity camera = main_scene->CreateEntity("camera");

        const engine::Entity camera_child = main_scene->CreateEntity("camera_child", camera, glm::vec3{0.0f, 0.0f, -3.0f});
        main_scene->GetTransform(camera_child)->is_static = false;
        const auto camren = main_scene->AddComponent<engine::MeshRenderableComponent>(camera_child);
        camren->visible = false;
        camren->mesh = GenSphereMesh(app.getRenderer()->GetDevice(), 1.0f, /*16*/ 32);
        camren->material = app.getResource<engine::Material>("builtin.default");

        /* as of right now, the entity with tag 'camera' is used to build the view
         * matrix */

        const auto camera_transform = main_scene->GetComponent<engine::TransformComponent>(camera);
        camera_transform->position = {0.0f, 0.0f, 100.0f};
        camera_transform->is_static = false;

        main_scene->RegisterComponent<CameraControllerComponent>();
        main_scene->RegisterSystemAtIndex<CameraControllerSystem>(0);
        main_scene->AddComponent<CameraControllerComponent>(camera);

        /* floor */
        // engine::Entity floor = engine::loadGLTF(*main_scene, app.getResourcePath("models/floor2.glb"), true);
        //  main_scene->GetScale(floor).x *= 100.0f;
        //  main_scene->GetScale(floor).z *= 100.0f;
        //  main_scene->GetComponent<engine::MeshRenderableComponent>(main_scene->GetEntity("Cube", floor))->visible = false;

        const engine::Entity monke = engine::loadGLTF(*main_scene, app.getResourcePath("models/monke.glb"), true);
        main_scene->GetComponent<engine::TransformComponent>(monke)->position.y += 10.0f;

        // engine::Entity bottle = engine::loadGLTF(*main_scene, app.getResourcePath("models/bottle.glb"));
        // main_scene->GetComponent<engine::TransformComponent>(bottle)->scale *= 10.0f;
        // main_scene->GetComponent<engine::TransformComponent>(bottle)->position.x += 25.0f;
        // main_scene->GetComponent<engine::TransformComponent>(bottle)->position.z += 5.0f;

        const engine::Entity helmet = engine::loadGLTF(*main_scene, app.getResourcePath("models/DamagedHelmet.glb"), true);
        main_scene->GetPosition(helmet) += glm::vec3{5.0f, 5.0f, 5.0f};
        main_scene->GetScale(helmet) *= 3.0f;
        main_scene->GetRotation(helmet) = glm::angleAxis(glm::pi<float>(), glm::vec3{0.0f, 0.0f, 1.0f});
        main_scene->GetRotation(helmet) *= glm::angleAxis(glm::half_pi<float>(), glm::vec3{1.0f, 0.0f, 0.0f});

        const engine::Entity toycar = engine::loadGLTF(*main_scene, app.getResourcePath("models/ToyCar.glb"), true);
        main_scene->GetScale(toycar) *= 150.0f;
        main_scene->GetPosition(toycar).z -= 0.07f;

        const engine::Entity stairs = engine::loadGLTF(*main_scene, app.getResourcePath("models/stairs.glb"), true);
        main_scene->GetPosition(stairs) += glm::vec3{-8.0f, -5.0f, 0.1f};
        main_scene->GetRotation(stairs) = glm::angleAxis(glm::half_pi<float>(), glm::vec3{0.0f, 0.0f, 1.0f});
        main_scene->GetRotation(stairs) *= glm::angleAxis(glm::half_pi<float>(), glm::vec3{1.0f, 0.0f, 0.0f});

        const engine::Entity axes = engine::loadGLTF(*main_scene, app.getResourcePath("models/MY_AXES.glb"), true);
        main_scene->GetPosition(axes) += glm::vec3{-40.0f, -40.0f, 1.0f};

        const engine::Entity bottle = engine::loadGLTF(*main_scene, app.getResourcePath("models/bottle.glb"), true);
        main_scene->GetPosition(bottle).y -= 10.0f;
        main_scene->GetPosition(bottle).z += 2.5f;
        main_scene->GetScale(bottle) *= 25.0f;

        // engine::Entity cube = engine::util::LoadGLTF(*main_scene, app.getResourcePath("models/cube.glb"), false);
        const engine::Entity cube = main_scene->CreateEntity("cube", 0, glm::vec3{4.0f, -17.0f, 0.0f});
        main_scene->GetTransform(cube)->is_static = false;
        const auto cube_ren = main_scene->AddComponent<engine::MeshRenderableComponent>(cube);
        cube_ren->material = app.getResource<engine::Material>("builtin.default");
        cube_ren->mesh = GenCuboidMesh(app.getRenderer()->GetDevice(), 1.0f, 1.0f, 1.0f);
        cube_ren->visible = true;
        const auto cubeCustom = main_scene->AddComponent<engine::CustomComponent>(cube);
        class Spinner : public engine::ComponentCustomImpl {
        private:
            float yaw = 0.0f;

        public:
            void init() override {}
            void update(float dt) override
            {
                yaw += dt;
                m_scene->GetRotation(m_entity) = glm::angleAxis(yaw, glm::vec3{0.0f, 0.0f, 1.0f});
                m_scene->GetRotation(m_entity) *= glm::angleAxis(glm::half_pi<float>(), glm::vec3{1.0f, 0.0f, 0.0f});
            }
        };
        cubeCustom->impl = std::make_unique<Spinner>();

        engine::Entity teapot = engine::loadGLTF(*main_scene, app.getResourcePath("models/teapot.glb"), true);
        main_scene->GetPosition(teapot).y += 5.0f;
        main_scene->GetPosition(teapot).x -= 5.0f;
        main_scene->GetScale(teapot) *= 5.0f;

        const engine::Entity tree = engine::loadGLTF(*main_scene, app.getResourcePath("models/tree.glb"), true);
        main_scene->GetPosition(tree) = glm::vec3{-5.0f, -5.0f, 0.0f};

        const engine::Entity box = engine::loadGLTF(*main_scene, app.getResourcePath("models/box.glb"), true);
        main_scene->GetPosition(box) = glm::vec3{-5.0f, -17.0f, 0.1f};
        main_scene->GetScale(box) *= 10.0f;
        main_scene->GetRotation(box) = glm::angleAxis(glm::pi<float>() * 0.0f, glm::vec3{0.0f, 0.0f, 1.0f});
        main_scene->GetRotation(box) *= glm::angleAxis(glm::half_pi<float>(), glm::vec3{1.0f, 0.0f, 0.0f});

        const auto land_material = std::make_shared<engine::Material>(app.getRenderer(), app.getResource<engine::Shader>("builtin.fancy"));
        land_material->setAlbedoTexture(engine::LoadTextureFromFile(
            "C:\\storage dump 2\\intromax\\myproject\\sceneassets\\images\\grass1-bl\\grass1-albedo3.png", engine::gfx::SamplerInfo{}, app.getRenderer()));
        land_material->setNormalTexture(
            engine::LoadTextureFromFile("C:\\storage dump 2\\intromax\\myproject\\sceneassets\\images\\grass1-bl\\grass1-normal1-ogl.png",
                                        engine::gfx::SamplerInfo{}, app.getRenderer(), false));
        land_material->setOcclusionRoughnessMetallicTexture(
            engine::LoadTextureFromFile("C:\\storage dump 2\\intromax\\myproject\\sceneassets\\images\\grass1-bl\\combined_ar_rough_metal.png",
                                        engine::gfx::SamplerInfo{}, app.getRenderer(), false));

        constexpr int LANDS_SIZE = 10;
        constexpr float LANDS_XY_SCALE = 50.0f;
        constexpr float LANDS_HEIGHT = 5.0f;
        constexpr float LANDS_UV_SCALE = 40.0f;
        constexpr unsigned int LANDS_SEED = 69420;
        const engine::Entity lands_parent = main_scene->CreateEntity("lands_parent");
        engine::Entity lands[LANDS_SIZE][LANDS_SIZE]{};
        for (int y = 0; y < LANDS_SIZE; ++y) {
            for (int x = 0; x < LANDS_SIZE; ++x) {
                lands[x][y] = main_scene->CreateEntity("land[" + std::to_string(x) + "][" + std::to_string(y) + "]", lands_parent);

                const auto land_ren = main_scene->AddComponent<engine::MeshRenderableComponent>(lands[x][y]);
                land_ren->mesh = genTerrainChunk(app.getRenderer()->GetDevice(), (float)x, (float)y, LANDS_UV_SCALE, LANDS_SEED);
                land_ren->material = land_material;
                land_ren->visible = true;

                const auto land_col = main_scene->AddComponent<engine::ColliderComponent>(lands[x][y]);
                land_col->aabb.min = glm::vec3{-std::numeric_limits<float>::epsilon(), -std::numeric_limits<float>::epsilon(), 0.0f};
                land_col->aabb.max = glm::vec3{1.0f + std::numeric_limits<float>::epsilon(), 1.0f + std::numeric_limits<float>::epsilon(), 0.01f};

                main_scene->GetPosition(lands[x][y]) = glm::vec3{(float)x * LANDS_XY_SCALE - (LANDS_XY_SCALE * 0.5f * (float)LANDS_SIZE),
                                                                 (float)y * LANDS_XY_SCALE - (LANDS_XY_SCALE * 0.5f * (float)LANDS_SIZE), 0.0f};
                main_scene->GetScale(lands[x][y]) = glm::vec3{LANDS_XY_SCALE, LANDS_XY_SCALE, LANDS_HEIGHT};
                main_scene->GetTransform(lands[x][y])->is_static = true;
            }
        }
    }

    start_scene->GetSystem<CameraControllerSystem>()->next_scene_ = main_scene;
    main_scene->GetSystem<CameraControllerSystem>()->next_scene_ = start_scene;

    app.getSceneManager()->SetActiveScene(main_scene);
    app.gameLoop();
}
