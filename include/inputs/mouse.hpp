#pragma once

namespace engine::inputs {

enum class MouseButton : int {
	M_LEFT,
	M_MIDDLE,
	M_RIGHT,
	M_X1,
	M_X2,
	M_INVALID=7,
	M_SIZE=7
};

enum class MouseAxis : int {
	X,
	Y,
	X_SCR,
	Y_SCR
};

}
