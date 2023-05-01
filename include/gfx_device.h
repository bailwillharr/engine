#ifndef ENGINE_INCLUDE_GFX_DEVICE_H_
#define ENGINE_INCLUDE_GFX_DEVICE_H_

#include <memory>

#include "gfx.h"

struct SDL_Window;  // <SDL_video.h>

namespace engine {

class GFXDevice {
 public:
  GFXDevice(const char* app_name, const char* app_version, SDL_Window* window,
            gfx::GraphicsSettings settings);

  GFXDevice(const GFXDevice&) = delete;
  GFXDevice& operator=(const GFXDevice&) = delete;
  ~GFXDevice();

  void GetViewportSize(uint32_t* w, uint32_t* h);

  gfx::DrawBuffer* BeginRender();

  void FinishRender(gfx::DrawBuffer* draw_buffer);

  void CmdBindPipeline(gfx::DrawBuffer* draw_buffer,
                       const gfx::Pipeline* pipeline);

  void CmdBindVertexBuffer(gfx::DrawBuffer* draw_buffer, uint32_t binding,
                           const gfx::Buffer* buffer);

  void CmdBindIndexBuffer(gfx::DrawBuffer* draw_buffer,
                          const gfx::Buffer* buffer);

  void CmdDrawIndexed(gfx::DrawBuffer* draw_buffer, uint32_t index_count,
                      uint32_t instance_count, uint32_t first_index,
                      int32_t vertex_offset, uint32_t first_instance);

  void CmdPushConstants(gfx::DrawBuffer* draw_buffer,
                        const gfx::Pipeline* pipeline, uint32_t offset,
                        uint32_t size, const void* data);

  void CmdBindDescriptorSet(gfx::DrawBuffer* draw_buffer,
                            const gfx::Pipeline* pipeline,
                            const gfx::DescriptorSet* set, uint32_t set_number);

  gfx::Pipeline* CreatePipeline(const gfx::PipelineInfo& info);

  void DestroyPipeline(const gfx::Pipeline* pipeline);

  gfx::DescriptorSetLayout* CreateDescriptorSetLayout(
      const std::vector<gfx::DescriptorSetLayoutBinding>& bindings);

  void DestroyDescriptorSetLayout(const gfx::DescriptorSetLayout* layout);

  gfx::DescriptorSet* AllocateDescriptorSet(
      const gfx::DescriptorSetLayout* layout);

  // This updates all copies of the descriptor.
  // This cannot be used after any frames have been renderered
  void UpdateDescriptorUniformBuffer(const gfx::DescriptorSet* set,
                                     uint32_t binding,
                                     const gfx::UniformBuffer* buffer,
                                     size_t offset, size_t range);

  void UpdateDescriptorCombinedImageSampler(const gfx::DescriptorSet* set,
                                            uint32_t binding,
                                            const gfx::Image* image,
                                            const gfx::Sampler* sampler);

  gfx::UniformBuffer* CreateUniformBuffer(uint64_t size,
                                          const void* initial_data);

  void DestroyUniformBuffer(const gfx::UniformBuffer* descriptor_buffer);

  void WriteUniformBuffer(gfx::UniformBuffer* buffer, uint64_t offset,
                          uint64_t size, const void* data);

  // Loads data into staging buffer and copies that into a single GPU buffer.
  gfx::Buffer* CreateBuffer(gfx::BufferType type, uint64_t size,
                            const void* data);

  void DestroyBuffer(const gfx::Buffer* buffer);

  gfx::Image* CreateImage(uint32_t w, uint32_t h, const void* image_data);

  void DestroyImage(const gfx::Image* image);

  const gfx::Sampler* CreateSampler(const gfx::SamplerInfo& info);

  void DestroySampler(const gfx::Sampler* sampler);

  uint64_t GetFrameCount();

  void LogPerformanceInfo();

  // wait until all the active GPU queues have finished working
  void WaitIdle();

 private:
  struct Impl;
  std::unique_ptr<Impl> pimpl;
};

}  // namespace engine

#endif
