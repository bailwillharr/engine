#pragma once

#include <functional>

namespace engine {

struct CustomComponent {
    std::function<void(void)> on_init;    // void on_init(void);
    std::function<void(float)> on_update; // void on_update(float ts);
};

} // namespace engine