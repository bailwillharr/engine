#include "resources/resource.hpp"

#include <log.hpp>

Resource::Resource(const std::filesystem::path& resPath, const std::string& type) : m_resourcePath(resPath), m_type(type)
{
	if (m_type != "mesh")
		TRACE("Creating {} resource: {}", type, resPath.filename().string());
}

Resource::~Resource()
{
	if (m_type != "mesh")
		TRACE("Destroyed {} resource: {}", m_type, m_resourcePath.filename().string());
}

std::string Resource::getType()
{
	return m_type;
}
