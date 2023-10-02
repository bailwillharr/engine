#pragma once

#include <string>

#include "scene.h"

namespace engine::util {

engine::Entity LoadGLTF(Scene& scene, const std::string& path, bool isStatic = false);

}