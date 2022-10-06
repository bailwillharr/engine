#pragma once

#include <cstdint>
#include <cstddef>

namespace engine {

	enum class ShaderType {
		VERTEX,
		FRAGMENT,
	};

	struct Shader {
		ShaderType type;
		uint32_t handle;
	};

	typedef uint32_t Program;

	enum class BufferType {
		VERTEX,
		INDEX,
	};

	struct Buffer {
		BufferType type;
		uint32_t handle;
	};

	enum class Primitive {
		POINTS,
		LINES,
		LINE_STRIP,
		TRIANGLES,
		TRIANGLE_STRIP,
	};

	enum class IndexBufferFormat {
		UNSIGNED_8_BITS,
		UNSIGNED_16_BITS,
		UNSIGNED_32_BITS,
	};

}
