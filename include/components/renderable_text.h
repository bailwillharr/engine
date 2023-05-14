#ifndef ENGINE_INCLUDE_COMPONENTS_RENDERABLE_TEXT_H_
#define ENGINE_INCLUDE_COMPONENTS_RENDERABLE_TEXT_H_

#include <memory>
#include <string>

#include "resources/texture.h"

namespace engine {

struct RenderableTextComponent {
  void SetText(const std::string& text) {
    text_ = text;
    invalidated_ = true;
  }

 private:
  std::string text_{"hello world"};
  bool invalidated_ =
      true;  // text has been changed, texture must be regenerated
  std::unique_ptr<resources::Texture> texture_{};
};

}  // namespace engine

#endif