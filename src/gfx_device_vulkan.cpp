#ifdef ENGINE_BUILD_VULKAN

#include "gfx_device.hpp"

#include "config.h"

#include "window.hpp"

#include <vulkan/vulkan.h>

#include <assert.h>
#include <iostream>

namespace engine::gfx {

	class Device::Impl {
		friend Device;

		VkInstance m_instance;

	};

	Device::Device(AppInfo appInfo, const Window& window)
	{
		VkResult res;

		int appVersionMajor, appVersionMinor, appVersionPatch;
		assert(versionFromCharArray(appInfo.version, &appVersionMajor, &appVersionMinor, &appVersionPatch));
		int engineVersionMajor, engineVersionMinor, engineVersionPatch;
		assert(versionFromCharArray(ENGINE_VERSION, &engineVersionMajor, &engineVersionMinor, &engineVersionPatch));

		VkApplicationInfo applicationInfo{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext = nullptr,
			.pApplicationName = appInfo.name,
			.applicationVersion = VK_MAKE_VERSION(appVersionMajor, appVersionMinor, appVersionPatch),
			.pEngineName = "engine",
			.engineVersion = VK_MAKE_VERSION(engineVersionMajor, engineVersionMinor, engineVersionPatch),
			.apiVersion = VK_VERSION_1_0,
		};

		VkInstanceCreateInfo instanceInfo{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.pApplicationInfo = nullptr,
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = nullptr,
			.enabledExtensionCount = 0,
			.ppEnabledExtensionNames = nullptr,
		};

		res = vkCreateInstance(&instanceInfo, nullptr, &pimpl->m_instance);
		std::cout << "ERROR CODE: " << res << std::endl;
		assert(res == VK_SUCCESS);

	}

	Device::~Device()
	{
		vkDestroyInstance(pimpl->m_instance, nullptr);
	}

}

#endif
