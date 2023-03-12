#pragma once

#include <SDL2/SDL_video.h>

#include <Volk/volk.h>

namespace engine {

	struct Instance {
		VkInstance instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	};

	enum class MessageSeverity {
		SEV_VERBOSE,
		SEV_INFO,
		SEV_WARNING,
		SEV_ERROR // windows.h defines ERROR annoyingly
	};

	Instance createVulkanInstance(SDL_Window* window, const char* appName, const char* appVersion, bool useValidation, MessageSeverity validationLevel = MessageSeverity::SEV_WARNING);
	void destroyVulkanInstance(Instance instance);

}