#pragma once

#include <cstdint>
#include <vector>

// Enums and structs for the graphics abstraction

namespace engine::gfx {

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
			vsync = false;
			msaaLevel = MSAALevel::MSAA_OFF;
		}

		bool vsync;
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
		uint32_t location;
		VertexAttribFormat format;
		uint32_t offset;
	};

	struct VertexFormat {
		uint32_t stride;
		std::vector<VertexAttribDescription> attributeDescriptions;
	};

	// handles (incomplete types)
	struct Pipeline;
	struct Buffer;
	struct Texture;

}
