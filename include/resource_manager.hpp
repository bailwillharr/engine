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
		virtual ~ResourceManager() {}
		ResourceManager(const ResourceManager&) = delete;
		ResourceManager& operator=(const ResourceManager&) = delete;

		std::shared_ptr<T> add(const std::string& name, std::unique_ptr<T>&& resource)
		{
			if (m_resources.contains(name) == false) {
				std::shared_ptr<T> ptr = std::move(resource);
				m_resources.emplace(name, ptr);
			}
			else {
				throw std::runtime_error("Cannot add a resource which already exists");
			}
			return m_resources.at(name).lock();
		}

		std::shared_ptr<T> get(const std::string& name)
		{
			if (m_resources.contains(name)) {
				std::weak_ptr<T> resource = m_resources.at(name);
				if (resource.expired() == false) {
					return resource.lock();
				}
			}
			return {};
		}

	private:
		// weak ptrs are used to check a resource's use count. If the use count of a resource hits 0, the resource can safely be deleted.
		std::unordered_map<std::string, std::weak_ptr<T>> m_resources{};

	};

}