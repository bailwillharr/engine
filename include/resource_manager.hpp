#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

namespace engine {

	class IResourceManager {
	public:
		virtual ~IResourceManager() = default;
	};

	template <class T>
	class ResourceManager : public IResourceManager {

	public:
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

		void addPersistent(const std::string& name, std::unique_ptr<T>&& resource)
		{
			m_persistentResources.push_back(add(name, std::move(resource)));
		}

		std::shared_ptr<T> get(const std::string& name)
		{
			if (m_resources.contains(name)) {
				std::weak_ptr<T> ptr = m_resources.at(name);
				if (ptr.expired() == false) {
					return ptr.lock();
				} else {
					m_resources.erase(name);
				}
			}
			// resource doesn't exist:
			throw std::runtime_error("Resource doesn't exist: " + name);
			return {};
		}

	private:
		std::unordered_map<std::string, std::weak_ptr<T>> m_resources{};
		std::vector<std::shared_ptr<T>> m_persistentResources{}; // This array owns persistent resources

	};

}
