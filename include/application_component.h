#pragma once

#include <string>

struct SDL_Window; // forward-dec

namespace engine {

class Application; // forward-dec

class ApplicationComponent {
    // a class that extends many classes in the engine to expose 'global' functionality to lower-tier classes

    // if multithreading is ever implemented, this class must do synchronization as its instantiations may run concurrently
   private:
    Application& app_;

   protected:
    ApplicationComponent(Application& app) : app_(app) {}

    std::string GetResourcePath(const std::string& relative_path) const;
    SDL_Window* GetWindowHandle() const;
    const char* GetAppName() const;
    const char* GetAppVersion() const;

   public:
    ApplicationComponent() = delete;
};

} // namespace engine