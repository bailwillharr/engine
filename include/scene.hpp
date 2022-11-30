#pragma once

#include "object.hpp"

namespace engine {

	class Scene : public Object {
	
	public:
		Scene();
		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;
		~Scene();

	private:

	};

}