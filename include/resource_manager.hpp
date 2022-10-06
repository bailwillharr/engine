#pragma once

#include "engine_api.h"

#include "resources/resource.hpp"

#include <vector>
#include <unordered_map>

// Doesn't own resources, only holds weak_ptrs

namespace engine {

	class ENGINE_API ResourceManager {

	public:
		ResourceManager();
		ResourceManager(const ResourceManager&) = delete;
		ResourceManager& operator=(const ResourceManager&) = delete;
		~ResourceManager() = default;

		template <class T>
		std::shared_ptr<T> create(const std::string& name);

		// creates the resource if it is not in the map or the weak_ptr has expired
		template <class T>
		std::shared_ptr<T> get(const std::string& name);

		std::unique_ptr<std::string> getResourcesListString();

		std::vector<std::weak_ptr<Resource>> getAllResourcesOfType(const std::string& type);

		std::filesystem::path getFilePath(const std::string& name);

	private:
		std::filesystem::path m_resourcesPath;
		std::unordered_map<std::string, std::weak_ptr<Resource>> m_resources;

	};

	template <class T>
	std::shared_ptr<T> ResourceManager::create(const std::string& name)
	{
		if (std::is_base_of<Resource, T>::value == false) {
			throw std::runtime_error("ResourceManager::create() error: specified type is not a subclass of 'Resource'");
		}
		auto resource = std::make_shared<T>(getFilePath(name));
		m_resources.emplace(name, std::dynamic_pointer_cast<Resource>(resource));
		return resource;
	}

	template <class T>
	std::shared_ptr<T> ResourceManager::get(const std::string& name)
	{

		if (std::is_base_of<Resource, T>::value == false) {
			throw std::runtime_error("ResourceManager::get() error: specified type is not a subclass of 'Resource'");
		}

		if (m_resources.contains(name)) {

			std::weak_ptr<Resource> res = m_resources.at(name);

			if (res.expired() == false) {
				// resource definitely exists
				auto castedSharedPtr = std::dynamic_pointer_cast<T>(res.lock());
				if (castedSharedPtr == nullptr) {
					throw std::runtime_error("error: attempt to get Resource which already exists as another type");
				}
				else {
					return castedSharedPtr;
				}
			}
			else {
				// Resource in map but no longer exists. Delete it.
				m_resources.erase(name);
			}

		}

		return create<T>(name);

	}

}