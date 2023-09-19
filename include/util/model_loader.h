#ifndef ENGINE_INCLUDE_UTIL_MODEL_LOADER_H_
#define ENGINE_INCLUDE_UTIL_MODEL_LOADER_H_

#include <string>

#include "scene.h"

namespace engine {
namespace util {

Entity LoadMeshFromFile(Scene* parent, const std::string& path, bool is_static = false);

}  // namespace util
}  // namespace engine

#endif