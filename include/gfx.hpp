#pragma once

#include <cstdint>
#include <vector>

namespace engine::gfx {

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

	struct VertexBufferDesc {
		uint64_t size;
	};

	struct VertexAttribDescription {
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
