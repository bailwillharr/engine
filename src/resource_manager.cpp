#include "resource_manager.hpp"

#ifdef _MSC_VER
#include <windows.h>
#define MAX_PATH 260
#endif

ResourceManager::ResourceManager()
{
#ifdef _MSC_VER
	CHAR exeDirBuf[MAX_PATH + 1];
	GetModuleFileNameA(NULL, exeDirBuf, MAX_PATH + 1);
	std::filesystem::path cwd = std::filesystem::path(exeDirBuf).parent_path();
#else
	std::filesystem::path cwd = std::filesystem::current_path();
#endif

	if (std::filesystem::is_directory(cwd / "res")) {
		m_resourcesPath = std::filesystem::absolute("res");
	} else {
		m_resourcesPath = cwd.parent_path() / "share" / "sdltest";
	}

	if (std::filesystem::is_directory(m_resourcesPath) == false) {
		m_resourcesPath = cwd.root_path() / "usr" / "local" / "share" / "sdltest";
	}

	if (std::filesystem::is_directory(m_resourcesPath) == false) {
		throw std::runtime_error("Unable to determine resources location");
	}
}

std::unique_ptr<std::string> ResourceManager::getResourcesListString()
{
	auto bufPtr = std::make_unique<std::string>();
	std::string& buf = *bufPtr;
	int maxLength = 0;
	for (const auto& [name, ptr] : m_resources) {
		if (name.length() > maxLength)
			maxLength = name.length();
	}
	for (const auto& [name, ptr] : m_resources) {
		buf += name;
		for (int i = 0; i < (maxLength - name.length() + 4); i++) {
			buf += " ";
		}
		buf += std::to_string(ptr.use_count()) + "\n";
	}
	return bufPtr;
}


std::vector<std::weak_ptr<Resource>> ResourceManager::getAllResourcesOfType(const std::string& type)
{
	std::vector<std::weak_ptr<Resource>> resources;
	for (const auto& [name, ptr] : m_resources) {
		if (ptr.expired() == false) {
			if (ptr.lock()->getType() == type) {
				resources.push_back(ptr);
			}
		}
	}
	return resources;
}

// private

std::filesystem::path ResourceManager::getFilePath(const std::string& name)
{
	return m_resourcesPath / name;
}
