#include "util/gltf_loader.h"

#include "log.h"
#include "util/files.h"

namespace engine::util {

engine::Entity LoadGLTF(Scene& scene, const std::string& path, bool isStatic)
{

    auto file = ReadTextFile(path);



    return static_cast<Entity>(0u);
}

} // namespace engine::util