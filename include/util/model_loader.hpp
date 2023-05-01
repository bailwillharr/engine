#ifndef ENGINE_INCLUDE_UTIL_MODEL_LOADER_H_
#define ENGINE_INCLUDE_UTIL_MODEL_LOADER_H_

#include <string>

#include "scene.hpp"

namespace engine {
namespace util {

uint32_t LoadMeshFromFile(Scene* parent, const std::string& path);

}  // namespace util
}  // namespace engine

#endif