// The implementation of the graphics layer using OpenGL 4.5
// This uses SDL specific code

#ifdef ENGINE_BUILD_OPENGL

#include "gfx_device.hpp"

#include "log.hpp"

#include <glad/glad.h>

#include <SDL_video.h>

#include <fstream>

namespace engine {

	// EXTERNED GLOBAL VARIABLE
	GFXDevice* gfxdev = nullptr;

	// structures and enums



	// handles

	struct gfx::Buffer {
		gfx::BufferType type;
		// TODO
	};

	struct gfx::Pipeline {
		// TODO
	};



	// enum converters

	namespace vkinternal {
		/*
		static GLenum getVertexAttribFormat(gfx::VertexAttribFormat fmt)
		{
			switch (fmt) {
			case gfx::VertexAttribFormat::VEC2:
				return VK_FORMAT_R32G32_SFLOAT;
			case gfx::VertexAttribFormat::VEC3:
				return VK_FORMAT_R32G32B32_SFLOAT;
			}
			throw std::runtime_error("Unknown vertex attribute format");
		}

		static VkBufferUsageFlagBits getBufferUsageFlag(gfx::BufferType type)
		{
			switch (type) {
			case gfx::BufferType::VERTEX:
				return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			case gfx::BufferType::INDEX:
				return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			}
			throw std::runtime_error("Unknown buffer type");
		}*/

	}

	// functions

	static std::vector<char> readFile(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (file.is_open() == false) {
			throw std::runtime_error("Unable to open file " + filename);
		}
		std::vector<char> buffer(file.tellg());
		file.seekg(0);
		file.read(buffer.data(), buffer.size());
		file.close();
		return buffer;
	}

	// class definitions

	struct GFXDevice::Impl {

		SDL_Window* window = nullptr;

		uint64_t FRAMECOUNT = 0;

		SDL_GLContext context;

	};

	GFXDevice::GFXDevice(const char* appName, const char* appVersion, SDL_Window* window)
	{
		if (gfxdev != nullptr) {
			throw std::runtime_error("There can only be one graphics device");
		}
		gfxdev = this;

		pimpl = std::make_unique<Impl>();

		pimpl->window = window;

		pimpl->context = SDL_GL_CreateContext(window);
		if (pimpl->context == NULL) {
			throw std::runtime_error("Unable to create OpenGL context: " + std::string(SDL_GetError()));
		}

		if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
			throw std::runtime_error("Unable to initialise GLAD");
		}

	}

	GFXDevice::~GFXDevice()
	{
		TRACE("Destroying GFXDevice...");

		SDL_GL_DeleteContext(pimpl->context);
	}

	void GFXDevice::getViewportSize(uint32_t* w, uint32_t* h)
	{
		int width, height;
		SDL_GL_GetDrawableSize(pimpl->window, &width, &height);
		*w = (uint32_t)width;
		*h = (uint32_t)height;
	}

	void draw(const gfx::Pipeline* pipeline, const gfx::Buffer* vertexBuffer, const gfx::Buffer* indexBuffer, uint32_t count, const void* pushConstantData, size_t pushConstantSize, const gfx::Texture* texture)
	{
		assert(vertexBuffer->type == gfx::BufferType::VERTEX);
		assert(vertexBuffer != nullptr);
		assert(indexBuffer == nullptr || indexBuffer->type == gfx::BufferType::INDEX);

	}

	void GFXDevice::renderFrame()
	{
		glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		SDL_GL_SwapWindow(pimpl->window);

		pimpl->FRAMECOUNT++;
	}

	gfx::Pipeline* GFXDevice::createPipeline(const char* vertShaderPath, const char* fragShaderPath, const gfx::VertexFormat& vertexFormat, uint64_t uniformBufferSize)
	{
		gfx::Pipeline* pipeline = new gfx::Pipeline{};

		return pipeline;
	}

	void GFXDevice::destroyPipeline(const gfx::Pipeline* pipeline)
	{
		delete pipeline;
	}

	void GFXDevice::updateUniformBuffer(const gfx::Pipeline* pipeline, void* data)
	{

	}

	gfx::Buffer* GFXDevice::createBuffer(gfx::BufferType type, uint64_t size, const void* data)
	{
		auto out = new gfx::Buffer{};

		out->type = type;

		return out;
	}

	void GFXDevice::destroyBuffer(const gfx::Buffer* buffer)
	{
		delete buffer;
	}

	void GFXDevice::waitIdle()
	{
		//glFinish();
	}

}

#endif
