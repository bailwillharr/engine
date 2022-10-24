#pragma once

#include <cstdint>
#include <cstddef>
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
		VEC2,
		VEC3,
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

}
