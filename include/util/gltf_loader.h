#pragma once

#include <string>

#include "scene.h"

namespace engine::util {

/*
 * Loads the default scene found in a glTF file into 'scene'.
 * 'isStatic' will mark every transform as static to aid rendering optimisation.
 * Returns the top-level glTF node as an engine entity.
 * 
 * Loader limitations:
 *  - Can only load .glb files
 *  - glTF files must contain all textures
 */
engine::Entity LoadGLTF(Scene& scene, const std::string& path, bool isStatic = false);

}