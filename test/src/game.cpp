#include "game.hpp"

#include "application.h"
#include "camera_controller.hpp"
#include "components/collider.h"
#include "components/custom.h"
#include "components/renderable.h"
#include "components/transform.h"
#include "input_manager.h"
#include "meshgen.hpp"
#include "resources/font.h"
#include "resources/material.h"
#include "resources/texture.h"
#include "scene.h"
#include "scene_manager.h"
#include "systems/render.h"
#include "systems/transform.h"
#include "util/model_loader.h"
#include "window.h"

#include "config.h"

static void ConfigureInputs(engine::InputManager* input_manager) {
  // user interface mappings
  input_manager->AddInputButton("fullscreen", engine::inputs::Key::K_F11);
  input_manager->AddInputButton("exit", engine::inputs::Key::K_ESCAPE);
  // game buttons
  input_manager->AddInputButton("fire", engine::inputs::MouseButton::M_LEFT);
  input_manager->AddInputButton("aim", engine::inputs::MouseButton::M_RIGHT);
  input_manager->AddInputButton("jump", engine::inputs::Key::K_SPACE);
  input_manager->AddInputButton("sprint", engine::inputs::Key::K_LSHIFT);
  // game movement
  input_manager->AddInputButtonAsAxis("movex", engine::inputs::Key::K_D,
                                      engine::inputs::Key::K_A);
  input_manager->AddInputButtonAsAxis("movey", engine::inputs::Key::K_W,
                                      engine::inputs::Key::K_S);
  // looking around
  input_manager->AddInputAxis("lookx", engine::inputs::MouseAxis::X);
  input_manager->AddInputAxis("looky", engine::inputs::MouseAxis::Y);
}

void PlayGame(GameSettings settings) {
  LOG_INFO("FPS limiter: {}", settings.enable_frame_limiter ? "ON" : "OFF");
  LOG_INFO("Graphics Validation: {}",
           settings.enable_validation ? "ON" : "OFF");

  engine::gfx::GraphicsSettings graphics_settings{};
  graphics_settings.enable_validation = settings.enable_validation;
  graphics_settings.vsync = true;
  graphics_settings.wait_for_present = false;
  graphics_settings.msaa_level = engine::gfx::MSAALevel::kOff;

  engine::Application app(PROJECT_NAME, PROJECT_VERSION, graphics_settings);
  app.SetFrameLimiter(settings.enable_frame_limiter);
  app.window()->SetRelativeMouseMode(true);
  ConfigureInputs(app.input_manager());

  auto my_scene = app.scene_manager()->CreateEmptyScene();

  /* create camera */
  {
    my_scene->RegisterComponent<CameraControllerComponent>();
    my_scene->RegisterSystem<CameraControllerSystem>();

    auto camera = my_scene->CreateEntity("camera");
    my_scene->GetComponent<engine::TransformComponent>(camera)->position = {
        0.0f, 10.0f, 0.0f};
    auto camera_collider =
        my_scene->AddComponent<engine::ColliderComponent>(camera);
    camera_collider->is_static = false;
    camera_collider->is_trigger = false;
    camera_collider->aabb = {{-0.2f, -1.5f, -0.2f},
                            {0.2f, 0.2f, 0.2f}};  // Origin is at eye level
    my_scene->AddComponent<CameraControllerComponent>(camera);
    auto render_system = my_scene->GetSystem<engine::RenderSystem>();
    render_system->SetCameraEntity(camera);
  }

  /* shared resources */
  auto grass_texture = std::make_shared<engine::resources::Texture>(
      &app.render_data_, app.GetResourcePath("textures/grass.jpg"),
      engine::resources::Texture::Filtering::kAnisotropic);

  auto space_texture = std::make_shared<engine::resources::Texture>(
      &app.render_data_, app.GetResourcePath("textures/space2.png"),
      engine::resources::Texture::Filtering::kAnisotropic);

  /* skybox */
  {
    uint32_t skybox = my_scene->CreateEntity("skybox");
    auto skybox_renderable =
        my_scene->AddComponent<engine::RenderableComponent>(skybox);
    skybox_renderable->material = std::make_unique<engine::resources::Material>(
        app.GetResource<engine::resources::Shader>("builtin.skybox"));
    skybox_renderable->material->texture_ = space_texture;
    skybox_renderable->mesh =
        GenCuboidMesh(app.gfxdev(), 10.0f, 10.0f, 10.0f, 1.0f, true);
    my_scene->GetComponent<engine::TransformComponent>(skybox)->position = {
        -5.0f, -5.0f, -5.0f};
  }

  /* floor */
  {
    uint32_t floor = my_scene->CreateEntity("floor");
    my_scene->GetComponent<engine::TransformComponent>(floor)->position =
        glm::vec3{-50.0f, -0.1f, -50.0f};
    auto floor_renderable =
        my_scene->AddComponent<engine::RenderableComponent>(floor);
    floor_renderable->material = std::make_shared<engine::resources::Material>(
        app.GetResource<engine::resources::Shader>("builtin.standard"));
    floor_renderable->material->texture_ = grass_texture;
    floor_renderable->mesh =
        GenCuboidMesh(app.gfxdev(), 100.0f, 0.1f, 100.0f, 100.0f);
    floor_renderable->shown = true;
    auto floor_collider =
        my_scene->AddComponent<engine::ColliderComponent>(floor);
    floor_collider->is_static = true;
    floor_collider->aabb = {{0.0f, 0.0f, 0.0f}, {100.0f, 0.1f, 100.0f}};
  }

  //engine::util::LoadMeshFromFile(my_scene,
  //                               app.GetResourcePath("models/test_scene.dae"));

  auto cobbleHouse = engine::util::LoadMeshFromFile(
      my_scene, app.GetResourcePath("models/cobble_house/cobble_house.dae"));
  my_scene->GetComponent<engine::TransformComponent>(cobbleHouse)->position += glm::vec3{
      33.0f, 0.1f, 35.0f};

  /* some text */
  {
    int width, height;
    auto bitmap = app.GetResource<engine::resources::Font>("builtin.mono")
                      ->GetTextBitmap("ABCDEFGHIJKLMNOPQRSTUVWXYZ12345", 1080.0f,
                                      width, height);

    uint32_t textbox = my_scene->CreateEntity("textbox");
    auto textbox_renderable =
        my_scene->AddComponent<engine::RenderableComponent>(textbox);
    textbox_renderable->material =
        std::make_unique<engine::resources::Material>(
            app.GetResource<engine::resources::Shader>("builtin.quad"));
    textbox_renderable->material->texture_ =
        std::make_unique<engine::resources::Texture>(
            &app.render_data_, bitmap->data(), width, height,
            engine::resources::Texture::Filtering::kAnisotropic);
    textbox_renderable->mesh = GenSphereMesh(app.gfxdev(), 1.0f, 5);
    my_scene->GetComponent<engine::TransformComponent>(textbox)->scale.y =
        (float)height / (float)width;
    textbox_renderable->shown = true;

    my_scene->AddComponent<engine::CustomComponent>(textbox)->onUpdate =
        [&](float ts) {
          (void)ts;
          static float time_elapsed;
          time_elapsed += ts;
          if (time_elapsed >= 1.0f) {
            time_elapsed = 0.0f;

            //LOG_INFO("Creating new bitmap...");
            //auto fpsBitmap =
            //    app.GetResource<engine::resources::Font>("builtin.mono")
            //        ->GetTextBitmap(std::to_string(ts), 768.0f, fpsWidth,
            //   fpsHeight);
            //LOG_INFO("Bitmap created! Loading into new texture...");
            //textbox_renderable->material->texture_ =
            //    std::make_unique<engine::resources::Texture>(
            //        &app.render_data_, fpsBitmap->data(), fpsWidth, fpsHeight,
            //        engine::resources::Texture::Filtering::kBilinear);
            //LOG_INFO("Texture created!");
          }
        };
  }

  app.GameLoop();
}
