#include "object.hpp"

#include "log.hpp"

namespace engine {

	int Object::s_next_id = 1000;

	Object::Object(const std::string& name, Object* parent, Scene* scene)
		: name(name), parent(parent), scene(scene)
	{
		s_next_id++;
	}

	Object::~Object() {}

	Object* Object::getChild(const std::string& name)
	{
		for (const auto& child : m_children) {
			if (name == child->name) {
				return child.get();
			}
		}
		return nullptr;
	}

	std::vector<Object*> Object::getChildren()
	{
		std::vector<Object*> newVector{};
		for (const auto& child : m_children) {
			newVector.push_back(child.get());
		}
		return newVector;
	}

	Object* Object::createChild(const std::string& name)
	{
		if (getChild(name) != nullptr) {
			ERROR("Attempt to create child object with existing name");
			return nullptr;
		}
		m_children.emplace_back(std::make_unique<Object>(name, this, scene));
		return m_children.back().get();
	}

	bool Object::deleteChild(const std::string& name)
	{
		for (auto itr = m_children.begin(); itr != m_children.end(); ++itr) {
			if ((*itr)->name == name) {
				m_children.erase(itr);
				return true;
			}
		}
		return false;
	}

	void Object::printTree(int level)
	{
		std::string buf;
		for (int i = 0; i < level; i++) {
			if (i + 1 == level) {
				buf += "\\_______";
			}
			else {
				buf += "        ";
			}
		}
		buf += name;
		INFO("{}", buf);
		for (const auto& child : this->getChildren()) {
			child->printTree(level + 1);
		}
	}

/*
	void Object::getAllSubComponents(struct CompList& compList, glm::mat4 parentTransform)
	{
		glm::mat4 objTransform{ 1.0f };

		auto t = transform;

		// rotation
		objTransform = glm::mat4_cast(t.rotation);

		// position
		reinterpret_cast<glm::vec3&>(objTransform[3]) = t.position;

		// scale (effectively applied first
		objTransform = glm::scale(objTransform, t.scale);

		glm::mat4 newTransform = parentTransform * objTransform;

		for (const auto& compUnq : m_components) {
			const auto comp = compUnq.get();
			switch (comp->getType()) {
			case Component::TypeEnum::CAMERA:
				compList.cameras.emplace_back(dynamic_cast<Camera*>(comp), newTransform);
				break;
			case Component::TypeEnum::RENDERER:
				compList.renderers.emplace_back(dynamic_cast<Renderer*>(comp), newTransform);
				break;
			case Component::TypeEnum::UI:
				compList.uis.emplace_back(dynamic_cast<UI*>(comp), newTransform);
				break;
			case Component::TypeEnum::CUSTOM:
				compList.customs.emplace_back(dynamic_cast<CustomComponent*>(comp), newTransform);
				break;
			default:
				break;
			}
		}
		for (const auto& child : m_children) {
			child->getAllSubComponents(compList, newTransform);
		}
	}
*/

}