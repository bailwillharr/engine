#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <stdexcept>

namespace engine {

	template <class T>
	class ResourceManager {

	public:
		ResourceManager() {}
		~ResourceManager() {}
		ResourceManager(const ResourceManager&) = delete;
		ResourceManager& operator=(const ResourceManager&) = delete;

		std::shared_ptr<T> add(const std::string& name, std::unique_ptr<T>&& resource)
		{
			if (m_resources.contains(name) == false) {
				std::shared_ptr<T> resourceSharedPtr(std::move(resource));
				m_resources.emplace(name, resourceSharedPtr);
				return resourceSharedPtr;
			}
			else {
				throw std::runtime_error("Cannot add a resource which already exists");
			}
		}

		std::shared_ptr<T> get(const std::string& name)
		{
			if (m_resources.contains(name)) {
				std::weak_ptr<T> ptr = m_resources.at(name);
				if (ptr.expired() == false) {
					return ptr.lock();
				}
			}
			return {};
		}

	private:
		std::unordered_map<std::string, std::weak_ptr<T>> m_resources{};

	};

}
