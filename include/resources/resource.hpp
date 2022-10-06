#pragma once

#include "engine_api.h"

#include <string>
#include <filesystem>

namespace engine {

	class ENGINE_API Resource {

	public:
		Resource(const std::filesystem::path& resPath, const std::string& type);
		Resource(const Resource&) = delete;
		Resource& operator=(const Resource&) = delete;
		virtual ~Resource() = 0;

		std::string getType();

	protected:
		std::filesystem::path m_resourcePath;

	private:
		const std::string m_type;

	};

}