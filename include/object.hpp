#pragma once

#include "engine_api.h"

#include "log.hpp"

#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

#include <list>
#include <vector>
#include <string>
#include <memory>

namespace engine {

	/* forward declarations */
	class Scene;

	class Component {

	};

	struct Transform {
		// Scale, rotate (XYZ), translate
		glm::vec3 position{ 0.0f };
		glm::quat rotation{};
		glm::vec3 scale{ 1.0f };
	};

	class Object {

	public:
		Object(const std::string& name, Object* parent, Scene* scene);
		Object(const Object&) = delete;
		Object& operator=(const Object&) = delete;
		~Object();

		/* methods */

		Object* getChild(const std::string& name);
		std::vector<Object*> getChildren();
		Object* createChild(const std::string& name);
		bool deleteChild(const std::string& name);

		void printTree(int level = 0);

		// Returns the component of type T
		// Returns nullptr if the component is not found.
		template<class T> T* getComponent();
		template<class T> T* createComponent();
		template<class T> bool deleteComponent();

		/* public member variables */

		const std::string name;
		
		Object* const parent;
		Scene* const scene;

		Transform transform;

	private:
		static int s_next_id;
		int m_id = s_next_id;

		// If nullptr, this is the root object
		std::list<std::unique_ptr<Object>> m_children{};
		std::list<std::unique_ptr<Component>> m_components{};


	};

	/* implementation of template functions */

	template<class T> T* Object::getComponent()
	{
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
			ERROR("Object::createComponent(): attempt to create a component with a non-component class");
			return nullptr;
		}
		if (getComponent<T>() != nullptr) {
			ERROR("Object::createComponent(): attempt to create a component that already exists on an object");
			return nullptr;
		}
		m_components.emplace_back(std::make_unique<T>(this));
		return dynamic_cast<T*>(m_components.back().get());
	}

	template<class T> bool Object::deleteComponent()
	{
		for (auto itr = m_components.begin(); itr != m_components.end(); ++itr) {
			if (dynamic_cast<T*>((*itr).get()) != nullptr) {
				m_components.erase(itr);
				return true;
			}
		}
		return false;
	}

}
