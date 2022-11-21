#pragma once

#include "object.hpp"

#include <string>

namespace engine::util {

	Object* loadAssimpMeshFromFile(Object* parent, const std::string& path);

}