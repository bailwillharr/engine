#pragma once

#include <memory>
#include <unordered_map>

#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>

#include "application_component.h"
#include "gfx_device.h"
#include "system_mesh_render.h"

namespace engine {

// A uniform struct that holds data of type T
template <typename T>
struct UniformDescriptor {
    const gfx::DescriptorSetLayout* layout;
    const gfx::DescriptorSet* set;
    struct UniformBufferData {
        T data;
    } uniform_buffer_data;
    gfx::UniformBuffer* uniform_buffer;
};

struct Line {
    glm::vec3 pos1;
    glm::vec3 pos2;
    glm::vec3 color;
};

class Renderer : private ApplicationComponent {
   public:
    Renderer(Application& app, gfx::GraphicsSettings settings);

    ~Renderer();

    // staticList can be nullptr to render nothing
    void Render(bool window_is_resized, glm::mat4 camera_transform, const RenderList* static_list, const RenderList* dynamic_list, const std::vector<Line>& debug_lines);

    // getters

    GFXDevice* GetDevice() { return device_.get(); }

    const gfx::DescriptorSetLayout* GetGlobalSetLayout() { return global_uniform.layout; }

    const gfx::DescriptorSetLayout* GetFrameSetLayout() { return frame_uniform.layout; }

    const gfx::DescriptorSetLayout* GetMaterialSetLayout() { return material_set_layout; }

    std::unordered_map<gfx::SamplerInfo, const gfx::Sampler*> samplers;

   private:
    std::unique_ptr<GFXDevice> device_;

    struct CameraSettings {
        float vertical_fov_radians = glm::radians(70.0f);
        float clip_near = 0.1f;
        float clip_far = 1000.0f;
    } camera_settings_;

    // ALL vertex shaders must begin with:
    /*
    layout(set = 0, binding = 0) uniform GlobalSetUniformBuffer {
        mat4 proj;
    } globalSetUniformBuffer;

    layout(set = 1, binding = 0) uniform FrameSetUniformBuffer {
        mat4 view;
    } frameSetUniformBuffer;

    layout( push_constant ) uniform Constants {
        mat4 model;
    } constants;
    */
    // ALL fragment shaders must begin with:
    /*
    layout(set = 2, binding = 0) uniform sampler2D materialSetAlbedoSampler;
    layout(set = 2, binding = 1) uniform sampler2D materialSetNormalSampler;
    layout(set = 2, binding = 2) uniform sampler2D materialSetOcclusionSampler;
    layout(set = 2, binding = 3) uniform sampler2D materialSetMetallicRoughnessSampler;
    */

    // in vertex shader
    struct GlobalUniformData {
        glm::mat4 proj;
        glm::mat4 lightSpaceMatrix;
    };
    UniformDescriptor<GlobalUniformData> global_uniform; // rarely updates; set 0 binding 0
    UniformDescriptor<glm::mat4> frame_uniform;          // updates once per frame; set 1 binding 0
    // in fragment shader
    const gfx::DescriptorSetLayout* material_set_layout; // set 2; set bound per material

    float viewport_aspect_ratio_ = 1.0f;

    const gfx::Pipeline* last_bound_pipeline_ = nullptr;

    struct DebugRenderingThings {
        const gfx::Pipeline* pipeline = nullptr;
        // have a simple vertex buffer with 2 points that draws a line
        const gfx::Buffer* vertex_buffer = nullptr;
        // shader will take 2 clip space xyzw coords as push constants to define the line
    } debug_rendering_things_{};

    gfx::Image* skybox_cubemap = nullptr;
    const gfx::Sampler* skybox_sampler = nullptr;
    const gfx::Pipeline* skybox_pipeline = nullptr;
    const gfx::Buffer* skybox_buffer = nullptr;

    gfx::Image* shadow_map = nullptr;
    const gfx::Sampler* shadow_map_sampler = nullptr;
    const gfx::Pipeline* shadow_pipeline = nullptr;

    bool rendering_started = false;

    void DrawRenderList(gfx::DrawBuffer* draw_buffer, const RenderList& render_list);
};

} // namespace engine
