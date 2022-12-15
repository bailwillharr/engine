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

		T* add(const std::string& name, std::unique_ptr<T>&& resource)
		{
			if (m_resources.contains(name) == false) {
				m_resources.emplace(name, std::move(resource));
			}
			else {
				throw std::runtime_error("Cannot add a resource which already exists");
			}
			return m_resources.at(name).get();
		}

		T* get(const std::string& name)
		{
			if (m_resources.contains(name)) {
				return m_resources.at(name).get();
			}
			return {};
		}

	private:
		std::unordered_map<std::string, std::unique_ptr<T>> m_resources{};

	};

}
