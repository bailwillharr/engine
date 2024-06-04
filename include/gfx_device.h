#pragma once

#include <memory>

#include "gfx.h"

struct SDL_Window; // <SDL_video.h>
struct ImDrawData; // "imgui/imgui.h"

namespace engine {

class GFXDevice {
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;

public:
    GFXDevice(const char* app_name, const char* app_version, SDL_Window* window, gfx::GraphicsSettings settings);
    GFXDevice(const GFXDevice&) = delete;

    ~GFXDevice();

    GFXDevice& operator=(const GFXDevice&) = delete;

    void getViewportSize(uint32_t* w, uint32_t* h);

    void changePresentMode(gfx::PresentMode mode);
    gfx::PresentMode getPresentMode();

    void setupImguiBackend();
    void shutdownImguiBackend();
    void cmdRenderImguiDrawData(gfx::DrawBuffer* draw_buffer, ImDrawData* draw_data);

    gfx::DrawBuffer* beginRender(bool window_resized);
    void finishRender(gfx::DrawBuffer* draw_buffer);

    gfx::DrawBuffer* beginShadowmapRender(gfx::Image* image);
    void finishShadowmapRender(gfx::DrawBuffer* draw_buffer, gfx::Image* image);

    void cmdBindPipeline(gfx::DrawBuffer* draw_buffer, const gfx::Pipeline* pipeline);
    void cmdBindVertexBuffer(gfx::DrawBuffer* draw_buffer, uint32_t binding, const gfx::Buffer* buffer);
    void cmdBindIndexBuffer(gfx::DrawBuffer* draw_buffer, const gfx::Buffer* buffer);
    void cmdDrawIndexed(gfx::DrawBuffer* draw_buffer, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset,
                        uint32_t first_instance);
    void cmdDraw(gfx::DrawBuffer* drawBuffer, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
    void cmdPushConstants(gfx::DrawBuffer* draw_buffer, const gfx::Pipeline* pipeline, uint32_t offset, uint32_t size, const void* data);
    void cmdBindDescriptorSet(gfx::DrawBuffer* draw_buffer, const gfx::Pipeline* pipeline, const gfx::DescriptorSet* set, uint32_t set_number);

    gfx::Pipeline* createPipeline(const gfx::PipelineInfo& info);
    void destroyPipeline(const gfx::Pipeline* pipeline);

    gfx::DescriptorSetLayout* createDescriptorSetLayout(const std::vector<gfx::DescriptorSetLayoutBinding>& bindings);
    void destroyDescriptorSetLayout(const gfx::DescriptorSetLayout* layout);
    gfx::DescriptorSet* allocateDescriptorSet(const gfx::DescriptorSetLayout* layout);
    void freeDescriptorSet(const gfx::DescriptorSet* set);
    void updateDescriptorUniformBuffer(const gfx::DescriptorSet* set, uint32_t binding, const gfx::UniformBuffer* buffer, size_t offset, size_t range);
    void updateDescriptorCombinedImageSampler(const gfx::DescriptorSet* set, uint32_t binding, const gfx::Image* image, const gfx::Sampler* sampler);

    gfx::UniformBuffer* createUniformBuffer(uint64_t size, const void* initial_data);
    void destroyUniformBuffer(const gfx::UniformBuffer* descriptor_buffer);
    void writeUniformBuffer(gfx::UniformBuffer* buffer, uint64_t offset, uint64_t size, const void* data);

    gfx::Buffer* createBuffer(gfx::BufferType type, uint64_t size, const void* data);
    void destroyBuffer(const gfx::Buffer* buffer);

    gfx::Image* createImage(uint32_t w, uint32_t h, gfx::ImageFormat input_format, const void* image_data);
    gfx::Image* createImageCubemap(uint32_t w, uint32_t h, gfx::ImageFormat input_format, const std::array<const void*, 6>& image_data);
    gfx::Image* createImageShadowmap();
    void destroyImage(const gfx::Image* image);

    const gfx::Sampler* createSampler(const gfx::SamplerInfo& info);
    void destroySampler(const gfx::Sampler* sampler);

    uint64_t getFrameCount();
    void logPerformanceInfo();

    void waitIdle();
};

} // namespace engine
