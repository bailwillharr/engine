#include "sceneroot.hpp"

#include "window.hpp"

#include "components/custom.hpp"
#include "components/camera.hpp"
#include "components/mesh_renderer.hpp"
#include "components/text_ui_renderer.hpp"

#include <glm/mat4x4.hpp>

#include <iostream>
#include <algorithm>

#include "log.hpp"

SceneRoot::SceneRoot(struct GameIO things) : Object("root", nullptr, *this, things)
{
}

SceneRoot::SceneRoot(const std::filesystem::path& file, struct GameIO things) : SceneRoot(things)
{
	// TODO: make this a resource
	//loadFromSceneFile(file);
}

SceneRoot::~SceneRoot()
{
}

// private methods

// public methods

void SceneRoot::updateStuff()
{

	using namespace components;
	using namespace glm;

	struct CompList compList{};

	getAllSubComponents(compList, glm::mat4{1.0f});

	// update
	
	for (const auto& [c, t] : compList.customs) {
		c->onUpdate(t);
	}

	// render

	for (const auto& [c, t] : compList.cameras) {
		for (int id : m_activeCameras) {
			if (c->getID() == id) {
				c->updateCam(t);
				for (const auto& [c, t] : compList.renderers) {
					c->render(t);
				}

				break;
			}
		}
	}

	for (const auto& [c, t] : compList.uis) {
		c->render(t);
	}

}

void SceneRoot::activateCam(int id)
{
	auto& v = m_activeCameras;

	if (std::find(v.begin(), v.end(), id) == v.end()) {
		v.push_back(id);
	}
}

void SceneRoot::deactivateCam(int id)
{
	auto& v = m_activeCameras;

	for (auto it = v.begin(); it != v.end(); it++) {
		if (*it == id) {
			v.erase(it);
		}
	}
}
