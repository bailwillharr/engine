#include "renderer.h"

#include <array>

#include "application_component.h"
#include "util/files.h"

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
        auto& binding1 = globalSetBindings.emplace_back();
        binding1.descriptor_type = gfx::DescriptorType::kCombinedImageSampler;
        binding1.stage_flags = gfx::ShaderStageFlags::kFragment;
        auto& binding2 = globalSetBindings.emplace_back();
        binding2.descriptor_type = gfx::DescriptorType::kCombinedImageSampler;
        binding2.stage_flags = gfx::ShaderStageFlags::kFragment;
    }
    global_uniform.layout = device_->CreateDescriptorSetLayout(globalSetBindings);
    global_uniform.set = device_->AllocateDescriptorSet(global_uniform.layout);
    // const glm::vec3 light_location = glm::vec3{-0.4278, 0.7923, 0.43502} * 10.0f;
    const glm::vec3 light_location = glm::vec3{10.0f, 0.0f, 10.0f};
    // const glm::mat4 light_proj = glm::orthoRH_ZO(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 50.0f);
    const glm::mat4 light_proj = glm::perspectiveFovRH_ZO(glm::radians(90.0f), 1.0f, 1.0f, 5.0f, 50.0f);
    const glm::mat4 light_view = glm::lookAtRH(light_location, glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 0.0f, 1.0f});
    global_uniform.uniform_buffer_data.data.proj = glm::mat4{1.0f};
    global_uniform.uniform_buffer_data.data.lightSpaceMatrix = light_proj * light_view;
    global_uniform.uniform_buffer = device_->CreateUniformBuffer(sizeof(global_uniform.uniform_buffer_data), &global_uniform.uniform_buffer_data);
    device_->UpdateDescriptorUniformBuffer(global_uniform.set, 0, global_uniform.uniform_buffer, 0, sizeof(global_uniform.uniform_buffer_data));
    // binding1 is updated towards the end of the constructor once the skybox texture is loaded
    // binding2 is updated just after that

    std::vector<gfx::DescriptorSetLayoutBinding> frameSetBindings;
    {
        auto& binding0 = frameSetBindings.emplace_back();
        binding0.descriptor_type = gfx::DescriptorType::kUniformBuffer;
        binding0.stage_flags = gfx::ShaderStageFlags::kVertex;
    }
    frame_uniform.layout = device_->CreateDescriptorSetLayout(frameSetBindings);
    frame_uniform.set = device_->AllocateDescriptorSet(frame_uniform.layout);
    frame_uniform.uniform_buffer_data.data = light_view;
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

    {
        gfx::PipelineInfo debug_pipeline_info{};
        debug_pipeline_info.vert_shader_path = GetResourcePath("engine/shaders/debug.vert");
        debug_pipeline_info.frag_shader_path = GetResourcePath("engine/shaders/debug.frag");
        debug_pipeline_info.vertex_format = debug_vertex_format;
        debug_pipeline_info.alpha_blending = false;
        debug_pipeline_info.face_cull_mode = gfx::CullMode::kCullNone; // probably ignored for line rendering
        debug_pipeline_info.write_z = false;                           // lines don't need the depth buffer
        // debug_pipeline_info.descriptor_set_layouts = empty;
        debug_pipeline_info.line_primitives = true;

        debug_rendering_things_.pipeline = device_->CreatePipeline(debug_pipeline_info);
    }

    // create the skybox cubemap and update global skybox combined-image-sampler
    {
        constexpr uint32_t cubemap_w = 2048;
        constexpr uint32_t cubemap_h = 2048;
        int w{}, h{};
        std::array<std::unique_ptr<std::vector<uint8_t>>, 6> face_unq_ptrs{};
        std::array<const void*, 6> face_unsafe_ptrs{};

        for (int face = 0; face < 6; ++face) {
            std::string path = std::string("engine/textures/skybox") + std::to_string(face) + std::string(".jpg");
            face_unq_ptrs[face] = util::ReadImageFile(GetResourcePath(path), w, h);
            if (cubemap_w != w || cubemap_h != h) throw std::runtime_error("Skybox textures must be 512x512!");
            face_unsafe_ptrs[face] = face_unq_ptrs[face]->data();
        }

        skybox_cubemap = device_->CreateImageCubemap(cubemap_w, cubemap_h, gfx::ImageFormat::kSRGB, face_unsafe_ptrs);
        gfx::SamplerInfo sampler_info{};
        sampler_info.magnify = gfx::Filter::kLinear;
        sampler_info.minify = gfx::Filter::kLinear;
        sampler_info.mipmap = gfx::Filter::kLinear;
        sampler_info.wrap_u = gfx::WrapMode::kClampToEdge;
        sampler_info.wrap_v = gfx::WrapMode::kClampToEdge;
        sampler_info.wrap_w = gfx::WrapMode::kClampToEdge;
        sampler_info.anisotropic_filtering = true;
        skybox_sampler = device_->CreateSampler(sampler_info);

        device_->UpdateDescriptorCombinedImageSampler(global_uniform.set, 1, skybox_cubemap, skybox_sampler);
    }

    // create skybox shader
    {
        gfx::VertexFormat vertex_format{};
        vertex_format.attribute_descriptions.emplace_back(0, gfx::VertexAttribFormat::kFloat3, 0);
        vertex_format.stride = 3 * sizeof(float);

        gfx::PipelineInfo pipeline_info{};
        pipeline_info.vert_shader_path = GetResourcePath("engine/shaders/skybox.vert");
        pipeline_info.frag_shader_path = GetResourcePath("engine/shaders/skybox.frag");
        pipeline_info.vertex_format = vertex_format;
        pipeline_info.alpha_blending = false;
        pipeline_info.face_cull_mode = gfx::CullMode::kCullBack;
        pipeline_info.write_z = false;
        pipeline_info.line_primitives = false;
        pipeline_info.descriptor_set_layouts.push_back(GetGlobalSetLayout());
        pipeline_info.descriptor_set_layouts.push_back(GetFrameSetLayout());

        skybox_pipeline = device_->CreatePipeline(pipeline_info);
    }

    // create skybox vertex buffer
    {
        std::vector<glm::vec3> v{};
        v.push_back({0.0f, 0.0f, 2.0f});
        v.push_back({0.0f, 0.0f, 0.0f});
        v.push_back({2.0f, 0.0f, 0.0f});
        v.push_back({2.0f, 0.0f, 0.0f});
        v.push_back({2.0f, 0.0f, 2.0f});
        v.push_back({0.0f, 0.0f, 2.0f});
        // back
        v.push_back({2.0f, 2.0f, 2.0f});
        v.push_back({2.0f, 2.0f, 0.0f});
        v.push_back({0.0f, 2.0f, 0.0f});
        v.push_back({0.0f, 2.0f, 0.0f});
        v.push_back({0.0f, 2.0f, 2.0f});
        v.push_back({2.0f, 2.0f, 2.0f});
        // left
        v.push_back({0.0f, 2.0f, 2.0f});
        v.push_back({0.0f, 2.0f, 0.0f});
        v.push_back({0.0f, 0.0f, 0.0f});
        v.push_back({0.0f, 0.0f, 0.0f});
        v.push_back({0.0f, 0.0f, 2.0f});
        v.push_back({0.0f, 2.0f, 2.0f});
        // right
        v.push_back({2.0f, 0.0f, 2.0f});
        v.push_back({2.0f, 0.0f, 0.0f});
        v.push_back({2.0f, 2.0f, 0.0f});
        v.push_back({2.0f, 2.0f, 0.0f});
        v.push_back({2.0f, 2.0f, 2.0f});
        v.push_back({2.0f, 0.0f, 2.0f});
        // top
        v.push_back({0.0f, 2.0f, 2.0f});
        v.push_back({0.0f, 0.0f, 2.0f});
        v.push_back({2.0f, 0.0f, 2.0f});
        v.push_back({2.0f, 0.0f, 2.0f});
        v.push_back({2.0f, 2.0f, 2.0f});
        v.push_back({0.0f, 2.0f, 2.0f});
        // bottom
        v.push_back({2.0f, 2.0f, 0.0f});
        v.push_back({2.0f, 0.0f, 0.0f});
        v.push_back({0.0f, 0.0f, 0.0f});
        v.push_back({0.0f, 0.0f, 0.0f});
        v.push_back({0.0f, 2.0f, 0.0f});
        v.push_back({2.0f, 2.0f, 0.0f});
        for (glm::vec3& pos : v) {
            pos.x -= 1.0f;
            pos.y -= 1.0f;
            pos.z -= 1.0f;
        }
        for (size_t i = 0; i < v.size(); i += 3) {
            std::swap(v[i], v[i + 2]);
        }

        skybox_buffer = device_->CreateBuffer(gfx::BufferType::kVertex, v.size() * sizeof(glm::vec3), v.data());
    }

    // shadow map pipeline
    gfx::VertexFormat shadowVertexFormat{};
    shadowVertexFormat.stride = sizeof(float) * 12;                                                 // using the full meshes so a lot of data is skipped
    shadowVertexFormat.attribute_descriptions.emplace_back(0, gfx::VertexAttribFormat::kFloat3, 0); // position
    gfx::PipelineInfo shadowPipelineInfo{};
    shadowPipelineInfo.vert_shader_path = GetResourcePath("engine/shaders/shadow.vert");
    shadowPipelineInfo.frag_shader_path = GetResourcePath("engine/shaders/shadow.frag");
    shadowPipelineInfo.vertex_format = shadowVertexFormat;
    shadowPipelineInfo.face_cull_mode = gfx::CullMode::kCullFront; // shadows care about back faces
    shadowPipelineInfo.alpha_blending = false;
    shadowPipelineInfo.write_z = true;
    shadowPipelineInfo.line_primitives = false;
    shadowPipelineInfo.depth_attachment_only = true;
    shadowPipelineInfo.descriptor_set_layouts.emplace_back(GetGlobalSetLayout());
    shadow_pipeline = device_->CreatePipeline(shadowPipelineInfo);

    // shadow map image and sampler
    shadow_map = device_->CreateShadowmapImage();
    gfx::SamplerInfo sampler_info{};
    sampler_info.magnify = gfx::Filter::kLinear;
    sampler_info.minify = gfx::Filter::kLinear;
    sampler_info.mipmap = gfx::Filter::kLinear;          // trilinear is apparently good for shadow maps
    sampler_info.wrap_u = gfx::WrapMode::kClampToBorder; // sampler reads 1.0 out of bounds which ensures no shadowing there
    sampler_info.wrap_v = gfx::WrapMode::kClampToBorder;
    sampler_info.wrap_w = gfx::WrapMode::kClampToBorder;
    sampler_info.anisotropic_filtering = false; // Copilot says not to use aniso for shadow maps
    shadow_map_sampler = device_->CreateSampler(sampler_info);
    device_->UpdateDescriptorCombinedImageSampler(global_uniform.set, 2, shadow_map, shadow_map_sampler);
};

Renderer::~Renderer()
{
    device_->DestroySampler(shadow_map_sampler);
    device_->DestroyImage(shadow_map);
    device_->DestroyPipeline(shadow_pipeline);

    device_->DestroyBuffer(skybox_buffer);
    device_->DestroyPipeline(skybox_pipeline);
    device_->DestroySampler(skybox_sampler);
    device_->DestroyImage(skybox_cubemap);

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
        global_uniform.uniform_buffer_data.data.proj = proj_matrix;
        device_->WriteUniformBuffer(global_uniform.uniform_buffer, 0, sizeof(global_uniform.uniform_buffer_data), &global_uniform.uniform_buffer_data);
    }

    const glm::mat4 view_matrix = glm::inverse(camera_transform);
    frame_uniform.uniform_buffer_data.data = view_matrix;
    device_->WriteUniformBuffer(frame_uniform.uniform_buffer, 0, sizeof(frame_uniform.uniform_buffer_data), &frame_uniform.uniform_buffer_data);
}

void Renderer::Render(const RenderList* static_list, const RenderList* dynamic_list, const std::vector<Line>& debug_lines)
{

    if (rendering_started == false) {
        // render to shadow map
        gfx::DrawBuffer* shadow_draw = device_->BeginShadowmapRender(shadow_map);
        device_->CmdBindPipeline(shadow_draw, shadow_pipeline);
        device_->CmdBindDescriptorSet(shadow_draw, shadow_pipeline, global_uniform.set, 0); // only need light space matrix
        if (static_list) { // only create shadow map with static meshes
            if (!static_list->empty()) {
                for (const auto& entry : *static_list) {
                    device_->CmdPushConstants(shadow_draw, shadow_pipeline, 0, sizeof(entry.model_matrix), &entry.model_matrix);
                    device_->CmdBindVertexBuffer(shadow_draw, 0, entry.vertex_buffer);
                    device_->CmdBindIndexBuffer(shadow_draw, entry.index_buffer);
                    device_->CmdDrawIndexed(shadow_draw, entry.index_count, 1, 0, 0, 0);
                }
            }
        }
        device_->FinishShadowmapRender(shadow_draw, shadow_map);
    }
    rendering_started = true;

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

    struct DebugPush {
        glm::vec4 pos1;
        glm::vec4 pos2;
        glm::vec3 color;
    };

    // draw skybox
    {
        device_->CmdBindPipeline(draw_buffer, skybox_pipeline);
        device_->CmdBindDescriptorSet(draw_buffer, skybox_pipeline, global_uniform.set, 0);
        device_->CmdBindDescriptorSet(draw_buffer, skybox_pipeline, frame_uniform.set, 1);
        device_->CmdBindVertexBuffer(draw_buffer, 0, skybox_buffer);
        device_->CmdDraw(draw_buffer, 36, 1, 0, 0);
    }

    // draw debug shit here
    device_->CmdBindPipeline(draw_buffer, debug_rendering_things_.pipeline);
    DebugPush push{};
    for (const Line& l : debug_lines) {
        push.pos1 = global_uniform.uniform_buffer_data.data.proj * frame_uniform.uniform_buffer_data.data * glm::vec4(l.pos1, 1.0f);
        push.pos2 = global_uniform.uniform_buffer_data.data.proj * frame_uniform.uniform_buffer_data.data * glm::vec4(l.pos2, 1.0f);
        push.color = l.color;
        device_->CmdPushConstants(draw_buffer, debug_rendering_things_.pipeline, 0, sizeof(DebugPush), &push);
        device_->CmdDraw(draw_buffer, 2, 1, 0, 0);
    }

    // also make a lil crosshair
    push.color = glm::vec3{1.0f, 1.0f, 1.0f};
    push.pos1 = glm::vec4(-0.05f, 0.0f, 0.0f, 1.0f);
    push.pos2 = glm::vec4(0.05f, 0.0f, 0.0f, 1.0f);
    device_->CmdPushConstants(draw_buffer, debug_rendering_things_.pipeline, 0, sizeof(DebugPush), &push);
    device_->CmdDraw(draw_buffer, 2, 1, 0, 0);
    push.pos1 = glm::vec4(0.0f, -0.05f, 0.0f, 1.0f);
    push.pos2 = glm::vec4(0.0f, 0.05f, 0.0f, 1.0f);
    device_->CmdPushConstants(draw_buffer, debug_rendering_things_.pipeline, 0, sizeof(DebugPush), &push);
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
