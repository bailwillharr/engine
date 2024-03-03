#include "renderer.h"

#include "application_component.h"

#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include "imgui/imgui.h"

namespace engine {

Renderer::Renderer(Application& app, gfx::GraphicsSettings settings) : ApplicationComponent(app)
{
    device_ = std::make_unique<GFXDevice>(GetAppName(), GetAppVersion(), GetWindowHandle(), settings);

    // sort out descriptor set layouts:
    std::vector<gfx::DescriptorSetLayoutBinding> globalSetBindings;
    {
        auto& binding0 = globalSetBindings.emplace_back();
        binding0.descriptor_type = gfx::DescriptorType::kUniformBuffer;
        binding0.stage_flags = gfx::ShaderStageFlags::kVertex;
    }
    global_uniform.layout = device_->CreateDescriptorSetLayout(globalSetBindings);
    global_uniform.set = device_->AllocateDescriptorSet(global_uniform.layout);
    global_uniform.uniform_buffer_data.data = glm::mat4{1.0f};
    global_uniform.uniform_buffer = device_->CreateUniformBuffer(sizeof(global_uniform.uniform_buffer_data), &global_uniform.uniform_buffer_data);
    device_->UpdateDescriptorUniformBuffer(global_uniform.set, 0, global_uniform.uniform_buffer, 0, sizeof(global_uniform.uniform_buffer_data));

    std::vector<gfx::DescriptorSetLayoutBinding> frameSetBindings;
    {
        auto& binding0 = frameSetBindings.emplace_back();
        binding0.descriptor_type = gfx::DescriptorType::kUniformBuffer;
        binding0.stage_flags = gfx::ShaderStageFlags::kVertex;
    }
    frame_uniform.layout = device_->CreateDescriptorSetLayout(frameSetBindings);
    frame_uniform.set = device_->AllocateDescriptorSet(frame_uniform.layout);
    frame_uniform.uniform_buffer_data.data = glm::mat4{1.0f};
    frame_uniform.uniform_buffer = device_->CreateUniformBuffer(sizeof(frame_uniform.uniform_buffer_data), &frame_uniform.uniform_buffer_data);
    device_->UpdateDescriptorUniformBuffer(frame_uniform.set, 0, frame_uniform.uniform_buffer, 0, sizeof(frame_uniform.uniform_buffer_data));

    std::vector<gfx::DescriptorSetLayoutBinding> materialSetBindings;
    gfx::DescriptorSetLayoutBinding materialSetBinding{};
    materialSetBinding.descriptor_type = gfx::DescriptorType::kCombinedImageSampler;
    materialSetBinding.stage_flags = gfx::ShaderStageFlags::kFragment;
    materialSetBindings.push_back(materialSetBinding); // albedo
    materialSetBindings.push_back(materialSetBinding); // normal
    materialSetBindings.push_back(materialSetBinding); // occlusion
    materialSetBindings.push_back(materialSetBinding); // metallic-roughness
    material_set_layout = device_->CreateDescriptorSetLayout(materialSetBindings);

    device_->SetupImguiBackend();

    gfx::VertexFormat debug_vertex_format{};
    debug_vertex_format.stride = 0;
    // debug_vertex_format.vertex_attrib_descriptions = empty

    gfx::PipelineInfo debug_pipeline_info{};
    debug_pipeline_info.vert_shader_path = GetResourcePath("engine/shaders/debug.vert");
    debug_pipeline_info.frag_shader_path = GetResourcePath("engine/shaders/debug.frag");
    debug_pipeline_info.vertex_format = debug_vertex_format;
    debug_pipeline_info.alpha_blending = false;
    debug_pipeline_info.backface_culling = false; // probably ignored for line rendering
    debug_pipeline_info.write_z = false; // lines don't need the depth buffer
    //debug_pipeline_info.descriptor_set_layouts = empty;
    debug_pipeline_info.line_primitives = true;

    debug_rendering_things_.pipeline = device_->CreatePipeline(debug_pipeline_info);
};

Renderer::~Renderer()
{
    device_->DestroyPipeline(debug_rendering_things_.pipeline);

    for (const auto& [info, sampler] : samplers) {
        device_->DestroySampler(sampler);
    }
    device_->DestroyDescriptorSetLayout(material_set_layout);

    device_->DestroyUniformBuffer(frame_uniform.uniform_buffer);
    device_->DestroyDescriptorSetLayout(frame_uniform.layout);

    device_->DestroyUniformBuffer(global_uniform.uniform_buffer);
    device_->DestroyDescriptorSetLayout(global_uniform.layout);
}

void Renderer::PreRender(bool window_is_resized, glm::mat4 camera_transform)
{
    if (window_is_resized) {
        uint32_t w, h;
        device_->GetViewportSize(&w, &h);
        viewport_aspect_ratio_ = (float)w / (float)h;
        const glm::mat4 proj_matrix =
            glm::perspectiveRH_ZO(camera_settings_.vertical_fov_radians, viewport_aspect_ratio_, camera_settings_.clip_near, camera_settings_.clip_far);
        /* update SET 0 (rarely changing uniforms)*/
        global_uniform.uniform_buffer_data.data = proj_matrix;
        device_->WriteUniformBuffer(global_uniform.uniform_buffer, 0, sizeof(global_uniform.uniform_buffer_data), &global_uniform.uniform_buffer_data);
    }

    // set camera view matrix uniform
    /* update SET 1 (per frame uniforms) */
    const glm::mat4 view_matrix = glm::inverse(camera_transform);
    frame_uniform.uniform_buffer_data.data = view_matrix;
    device_->WriteUniformBuffer(frame_uniform.uniform_buffer, 0, sizeof(frame_uniform.uniform_buffer_data), &frame_uniform.uniform_buffer_data);
}

void Renderer::Render(const RenderList* static_list, const RenderList* dynamic_list)
{
    last_bound_pipeline_ = nullptr;

    gfx::DrawBuffer* draw_buffer = device_->BeginRender();

    if (static_list) {
        if (!static_list->empty()) {
            DrawRenderList(draw_buffer, *static_list);
        }
    }
    if (dynamic_list) {
        if (!dynamic_list->empty()) {
            DrawRenderList(draw_buffer, *dynamic_list);
        }
    }

    // draw debug shit here
    device_->CmdBindPipeline(draw_buffer, debug_rendering_things_.pipeline);
    glm::vec4 debug_positions[2] = {};
    debug_positions[0] = global_uniform.uniform_buffer_data.data * frame_uniform.uniform_buffer_data.data * glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
    debug_positions[1] = global_uniform.uniform_buffer_data.data * frame_uniform.uniform_buffer_data.data * glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f };
    device_->CmdPushConstants(draw_buffer, debug_rendering_things_.pipeline, 0, sizeof(glm::vec4) * 2, debug_positions);
    device_->CmdDraw(draw_buffer, 2, 1, 0, 0);

    device_->CmdRenderImguiDrawData(draw_buffer, ImGui::GetDrawData());

    device_->FinishRender(draw_buffer);
}

void Renderer::DrawRenderList(gfx::DrawBuffer* draw_buffer, const RenderList& render_list)
{
    // if a pipeline hasn't been bound yet at all
    if (last_bound_pipeline_ == nullptr) {
        const gfx::Pipeline* first_pipeline = render_list.begin()->pipeline;
        // these bindings persist between all pipelines
        device_->CmdBindDescriptorSet(draw_buffer, first_pipeline, global_uniform.set, 0);
        device_->CmdBindDescriptorSet(draw_buffer, first_pipeline, frame_uniform.set, 1);
        device_->CmdBindPipeline(draw_buffer, first_pipeline);
        last_bound_pipeline_ = first_pipeline;
    }

    for (const auto& entry : render_list) {
        if (entry.pipeline != last_bound_pipeline_) {
            device_->CmdBindPipeline(draw_buffer, entry.pipeline);
            last_bound_pipeline_ = entry.pipeline;
        }
        device_->CmdBindDescriptorSet(draw_buffer, entry.pipeline, entry.material_set, 2);
        device_->CmdPushConstants(draw_buffer, entry.pipeline, 0, sizeof(entry.model_matrix), &entry.model_matrix);
        device_->CmdBindVertexBuffer(draw_buffer, 0, entry.vertex_buffer);
        device_->CmdBindIndexBuffer(draw_buffer, entry.index_buffer);
        device_->CmdDrawIndexed(draw_buffer, entry.index_count, 1, 0, 0, 0);
    }
}

} // namespace engine
