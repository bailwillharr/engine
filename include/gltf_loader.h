#pragma once

#include <string>

#include "scene.h"

namespace engine {

engine::Entity loadGLTF(Scene& scene, const std::string& path, bool isStatic = false);

} // namespace engine