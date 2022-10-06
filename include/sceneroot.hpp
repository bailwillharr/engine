#pragma once

#include "engine_api.h"

#include "object.hpp"

#include <filesystem>

namespace engine {

	// Holds everything you would expect to find in a game scene
	class ENGINE_API SceneRoot : public Object {

	public:
		// create a new empty scene
		SceneRoot(struct GameIO things);
		SceneRoot(const std::filesystem::path& file, struct GameIO things);

		SceneRoot(const SceneRoot&) = delete;
		SceneRoot& operator=(const SceneRoot&) = delete;
		~SceneRoot();

		void updateStuff();

		void activateCam(int id);
		void deactivateCam(int id);

	private:
		std::vector<int> m_activeCameras{};

	};

}