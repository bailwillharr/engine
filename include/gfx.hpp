#pragma once

#include <cstdint>
#include <vector>
#include <type_traits>

// Enums and structs for the graphics abstraction

namespace engine::gfx {

	// handles (incomplete types)
	struct Pipeline;
	struct UniformBuffer;
	struct Buffer;
	struct DrawBuffer;
	struct DescriptorSetLayout;
	struct DescriptorSet;
	struct Image;
	struct Sampler;

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
			enableValidation = true;
			vsync = true;
			waitForPresent = true; // not all GPUs/drivers support immediate present with V-sync enabled
			msaaLevel = MSAALevel::MSAA_OFF;
		}

		bool enableValidation;
		bool vsync;
		// idle CPU after render until the frame has been presented (no affect with V-sync disabled)
		bool waitForPresent;
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

	enum class DescriptorType {
		UNIFORM_BUFFER,
		COMBINED_IMAGE_SAMPLER,
	};

	namespace ShaderStageFlags {
		enum Bits : uint32_t {
			VERTEX = 1 << 0,
			FRAGMENT = 1 << 1,
		};
		typedef std::underlying_type<Bits>::type Flags;
	}

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
		VertexFormat vertexFormat;
		bool alphaBlending;
		bool backfaceCulling;
		std::vector<const DescriptorSetLayout*> descriptorSetLayouts;
	};

	struct DescriptorSetLayoutBinding {
		DescriptorType descriptorType = DescriptorType::UNIFORM_BUFFER;
		ShaderStageFlags::Flags stageFlags = 0;
	};

}
