#pragma once

#include <cstdint>
#include <cstddef>

namespace engine {

	enum class ShaderType {
		VERTEX,
		FRAGMENT,
	};

	enum class BufferType {
		VERTEX,
		INDEX,
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
