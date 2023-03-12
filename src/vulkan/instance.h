#pragma once

#include <SDL2/SDL_video.h>

#include <Volk/volk.h>

namespace engine {

	struct Instance {
		VkInstance instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	};

	Instance createVulkanInstance(SDL_Window* window, const char* appName, const char* appVersion);
	void destroyVulkanInstance(Instance instance);

}