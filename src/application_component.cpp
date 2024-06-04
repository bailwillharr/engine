#include "application_component.h"

#include "application.h"
#include "window.h"

#include <string>

namespace engine {

std::string ApplicationComponent::getResourcePath(const std::string& relative_path) const { return m_app.getResourcePath(relative_path); }

SDL_Window* ApplicationComponent::getWindowHandle() const {
	return m_app.getWindow()->GetHandle();
}

const char* ApplicationComponent::getAppName() const {
	return m_app.app_name;
}
const char* ApplicationComponent::getAppVersion() const {
	return m_app.app_version;
}

} // namespace engine