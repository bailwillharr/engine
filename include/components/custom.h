#ifndef ENGINE_INCLUDE_COMPONENTS_CUSTOM_H_
#define ENGINE_INCLUDE_COMPONENTS_CUSTOM_H_

#include <functional>

namespace engine {

struct CustomComponent {
  std::function<void(void)> onInit;  // void onInit(void);
  std::function<void(float)> onUpdate;  // void onUpdate(float ts);
};

}  // namespace engine

#endif