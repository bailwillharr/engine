#pragma once

#include <string>

struct SDL_Window; // forward-dec

namespace engine {

class Application; // forward-dec

// a class that extends many classes in the engine to expose 'global' functionality
class ApplicationComponent {
private:
    Application& m_app;

public:
    ApplicationComponent() = delete;
    ApplicationComponent(const ApplicationComponent&) = delete;

    ~ApplicationComponent() = default;

    ApplicationComponent& operator=(const ApplicationComponent&) = delete;

protected:
    ApplicationComponent(Application& app) : m_app(app) {}

    std::string getResourcePath(const std::string& relative_path) const;
    SDL_Window* getWindowHandle() const;
    const char* getAppName() const;
    const char* getAppVersion() const;
};

} // namespace engine