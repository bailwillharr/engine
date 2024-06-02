#include "application_component.h"

#include "application.h"
#include "window.h"

#include <string>

namespace engine {

std::string ApplicationComponent::GetResourcePath(const std::string& relative_path) const { return app_.getResourcePath(relative_path); }

SDL_Window* ApplicationComponent::GetWindowHandle() const {
	return app_.window()->GetHandle();
}

const char* ApplicationComponent::GetAppName() const {
	return app_.app_name;
}
const char* ApplicationComponent::GetAppVersion() const {
	return app_.app_version;
}

} // namespace engine