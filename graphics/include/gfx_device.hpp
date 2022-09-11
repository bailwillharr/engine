#pragma once

#include "gfx_api.h"

#include "gfx.hpp"

namespace gfx {

	struct GFX_API Device {

		virtual void setViewport(uint32_t top_left_x, uint32_t top_left_y, uint32_t width, uint32_t height) = 0;

		virtual bool createShader(ShaderType type, const char* source, Shader& out) = 0;
		virtual bool createProgram(int count, const Shader* shaders, Program& out) = 0;

		virtual bool createBuffer(BufferType type, const void* data, Buffer& out) = 0;
		virtual void bufferData(const void* data, Buffer buffer) = 0;
		virtual void bufferSubData(uint32_t offset, uint32_t size, const void* data, Buffer buffer) = 0;

		virtual void drawElements(Primitive primitive, IndexBufferFormat format, uint32_t count, uint32_t offset);
		virtual void drawArrays() = 0;

	};



}
