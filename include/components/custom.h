#pragma once

#include <functional>

namespace engine {

struct CustomComponent {
    std::function<void(void)> onInit;    // void onInit(void);
    std::function<void(float)> onUpdate; // void onUpdate(float ts);
};

} // namespace engine