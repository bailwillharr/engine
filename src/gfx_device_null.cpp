// The implementation of the graphics layer using Vulkan 1.3.

#ifdef ENGINE_BUILD_NULL

#include "gfx_device.hpp"

#include <assert.h>
#include <unordered_set>
#include <array>
#include <fstream>
#include <filesystem>
#include <optional>
#include <queue>
#include <map>
#include <iostream>

namespace engine {

	struct GFXDevice::Impl {
		int FRAMECOUNT = 0;
	};

	GFXDevice::GFXDevice(const char* appName, const char* appVersion, SDL_Window* window, bool vsync)
	{
		(void)appName;
		(void)appVersion;
		(void)window;
		(void)vsync;

		pimpl = std::make_unique<Impl>();

	}

	GFXDevice::~GFXDevice()
	{
	}

	void GFXDevice::getViewportSize(uint32_t *w, uint32_t *h)
	{
		*w = 1024;
		*h = 768;
	}

	void GFXDevice::draw(const gfx::Pipeline* pipeline, const gfx::Buffer* vertexBuffer, const gfx::Buffer* indexBuffer, uint32_t count, const void* pushConstantData, size_t pushConstantSize, const gfx::Texture* texture)
	{
		(void)pipeline;
		(void)vertexBuffer;
		(void)indexBuffer;
		(void)count;
		(void)pushConstantData;
		(void)pushConstantSize;
		(void)texture;
	}

	void GFXDevice::renderFrame()
	{
		pimpl->FRAMECOUNT++;
	}
	
	gfx::Pipeline* GFXDevice::createPipeline(const char* vertShaderPath, const char* fragShaderPath, const gfx::VertexFormat& vertexFormat, uint64_t uniformBufferSize, bool alphaBlending, bool backfaceCulling)
	{
		(void)vertShaderPath;
		(void)fragShaderPath;
		(void)vertexFormat;
		(void)uniformBufferSize;
		(void)alphaBlending;
		(void)backfaceCulling;
		return nullptr;
	}

	void GFXDevice::destroyPipeline(const gfx::Pipeline* pipeline)
	{
		(void)pipeline;
	}

	void GFXDevice::updateUniformBuffer(const gfx::Pipeline* pipeline, const void* data, size_t size, uint32_t offset)
	{
		(void)pipeline;
		(void)data;
		(void)size;
		(void)offset;
	}

	gfx::Buffer* GFXDevice::createBuffer(gfx::BufferType type, uint64_t size, const void* data)
	{
		(void)type;
		(void)size;
		(void)data;
		return nullptr;
	}

	void GFXDevice::destroyBuffer(const gfx::Buffer* buffer)
	{
		(void)buffer;
	}

	gfx::Texture* GFXDevice::createTexture(
		const void* imageData,
		uint32_t width,
		uint32_t height,
		gfx::TextureFilter minFilter,
		gfx::TextureFilter magFilter,
		gfx::MipmapSetting mipmapSetting,
		bool useAnisotropy)
	{
		(void)imageData;
		(void)width;
		(void)height;
		(void)minFilter;
		(void)magFilter;
		(void)mipmapSetting;
		(void)useAnisotropy;
		return nullptr;
	}

	void GFXDevice::destroyTexture(const gfx::Texture* texture)
	{
		(void)texture;
	}

	void GFXDevice::waitIdle()
	{
	}

}

#endif
