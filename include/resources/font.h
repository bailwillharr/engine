#ifndef ENGINE_INCLUDE_RESOURCES_FONT_H_
#define ENGINE_INCLUDE_RESOURCES_FONT_H_

#include <string>

namespace engine {
namespace resources {

class Font {
 public:

  Font(const std::string& path);
  ~Font();
  Font(const Font&) = delete;
  Font& operator=(const Font&) = delete;

 private:
};

}  // namespace resources
}  // namespace engine

#endif