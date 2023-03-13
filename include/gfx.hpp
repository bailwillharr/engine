#pragma once

#include <cstdint>
#include <vector>

// Enums and structs for the graphics abstraction

namespace engine::gfx {

	// handles (incomplete types)
	struct Pipeline;
	struct Buffer;
	struct Texture;
	struct DrawBuffer;
	struct DescriptorSetLayout;
	struct DescriptorSet;

	enum class MSAALevel {
		MSAA_OFF,
		MSAA_2X,
		MSAA_4X,
		MSAA_8X,
		MSAA_16X,
	};

	struct GraphicsSettings {
		
		GraphicsSettings()
		{
			// sane defaults
			vsync = true;
			waitForPresent = true; // not all GPUs/drivers support immediate present with V-sync enabled
			msaaLevel = MSAALevel::MSAA_OFF;
		}

		bool vsync;
		bool waitForPresent; // idle CPU after render until the frame has been presented (no affect with V-sync disabled)
		MSAALevel msaaLevel;

	};

	enum class ShaderType {
		VERTEX,
		FRAGMENT,
	};

	enum class BufferType {
		VERTEX,
		INDEX,
		UNIFORM,
	};

	enum class Primitive {
		POINTS,
		LINES,
		LINE_STRIP,
		TRIANGLES,
		TRIANGLE_STRIP,
	};

	enum class VertexAttribFormat {
		FLOAT2,
		FLOAT3,
		FLOAT4
	};

	enum class TextureFilter {
		LINEAR,
		NEAREST,
	};

	enum class MipmapSetting {
		OFF,
		NEAREST,
		LINEAR,
	};

	struct VertexBufferDesc {
		uint64_t size;
	};

	struct VertexAttribDescription {
		VertexAttribDescription(uint32_t location, VertexAttribFormat format, uint32_t offset) :
			location(location),
			format(format),
			offset(offset) {}
		uint32_t location; // the index to use in the shader
		VertexAttribFormat format;
		uint32_t offset;
	};

	struct VertexFormat {
		uint32_t stride;
		std::vector<VertexAttribDescription> attributeDescriptions;
	};

	struct PipelineInfo {
		const char* vertShaderPath;
		const char* fragShaderPath;
		gfx::VertexFormat vertexFormat;
		bool alphaBlending;
		bool backfaceCulling;
		std::vector<const DescriptorSetLayout*> descriptorSetLayouts;
	};

}
