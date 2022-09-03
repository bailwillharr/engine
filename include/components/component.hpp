#pragma once

#include "export.h"

class Object;
class Window;
class Input;
class ResourceManager;

class ENGINE_API Component {

public:

	enum class TypeEnum {
		TRANSFORM,
		CAMERA,
		RENDERER,
		UI,
		CUSTOM,
	};

	Component(Object* parent, TypeEnum type);
	Component(const Component&) = delete;
	Component& operator=(const Component&) = delete;
	virtual ~Component() = 0;
	
	int getID();
	TypeEnum getType();

	Object& parent;

protected:
	Window& win;
	Input& inp;
	ResourceManager& res;

private:
	static int s_next_component_id;

	int m_id = s_next_component_id;
	TypeEnum m_type;

};
