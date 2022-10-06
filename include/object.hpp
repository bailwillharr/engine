#pragma once

#include "engine_api.h"

#include <glm/mat4x4.hpp>

#include "transform.hpp"

#include <list>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

namespace engine {
	class Window;
	class Input;

	class ResourceManager;

	class SceneRoot;
	class Component;

	namespace components {
		class Transform;
		class Camera;
		class Renderer;
		class UI;
		class CustomComponent;
	}

	struct GameIO {
		engine::Window* const win;
		engine::Input* const input;
		ResourceManager* const resMan;
	};

	// This object lives until it is deleted by its parent(s) or finally when the "Scene" is destroyed.
	// Therefore it is safe to return raw pointers
	class ENGINE_API Object {

	public:
		Object(std::string name, Object* parent, SceneRoot& root, struct GameIO things);
		Object(const Object&) = delete;
		Object& operator=(const Object&) = delete;
		~Object();

		engine::Window& win;
		engine::Input& inp;
		ResourceManager& res;

		SceneRoot& root;

		std::string getName();

		Object* getParent();

		Object* getChild(std::string name);
		std::vector<Object*> getChildren();

		Object* createChild(std::string name);
		void deleteChild(std::string name);

		void printTree(int level = 0);

		// Returns the component of type T
		// Returns nullptr if the component is not found.
		template<class T> T* getComponent();

		// returns the component added
		template<class T> T* createComponent();

		template<class T> void deleteComponent();

		struct CompList {
			std::vector<std::pair<components::Camera*, glm::mat4>> cameras;
			std::vector<std::pair<components::Renderer*, glm::mat4>> renderers;
			std::vector<std::pair<components::UI*, glm::mat4>> uis;
			std::vector<std::pair<components::CustomComponent*, glm::mat4>> customs;
		};

		// Adds to the provided vector all components of this object and of its children recursively.
		// Ignores 'Transform'
		void getAllSubComponents(struct CompList& compList, glm::mat4 t);

		Transform transform;

	private:
		static int s_object_count;
		int m_id = s_object_count;
		std::string m_name;

		std::list<std::unique_ptr<Object>> m_children{};
		std::list<std::unique_ptr<Component>> m_components{};

		// If nullptr, this is the root object
		Object* const m_parent;
		struct GameIO m_gameIO;

	};

	// implementation of template functions

	template<class T> T* Object::getComponent()
	{
		if (std::is_base_of<Component, T>::value == false) {
			throw std::runtime_error("getComponent() error: specified type is not a subclass of 'Component'");
		}
		for (const auto& component : m_components) {
			T* derived = dynamic_cast<T*>(component.get());
			if (derived != nullptr) {
				return derived;
			}
		}
		return nullptr;
	}

	template <class T> T* Object::createComponent()
	{
		if (std::is_base_of<Component, T>::value == false) {
			throw std::runtime_error("addComponent() error: specified type is not a subclass of 'Component'");
		}
		if (getComponent<T>() != nullptr) {
			throw std::runtime_error("addComponent() error: attempt to add component of a type already present");
		}
		m_components.emplace_back(std::make_unique<T>(this));
		return dynamic_cast<T*>(m_components.back().get());
	}

	template<class T> void Object::deleteComponent()
	{
		if (std::is_base_of<Component, T>::value == false) {
			throw std::runtime_error("deleteComponent() error: specified type is not a subclass of 'Component'");
		}
		for (auto itr = m_components.begin(); itr != m_components.end(); ++itr) {
			if (dynamic_cast<T*>((*itr).get()) != nullptr) {
				m_components.erase(itr);
				return;
			}
		}
		throw std::runtime_error("deleteComponent() error: attempt to delete component that is not present.");
	}

}