#pragma once

#include <set>
#include <vector>
#include <map>
#include <string>
#include <bitset>
#include <cstdint>
#include <assert.h>

namespace engine {

	class Scene;

	constexpr size_t MAX_COMPONENTS = 64;

	class IComponentArray {
	public:
		virtual ~IComponentArray() = default;
	};

	template<typename T>
	class ComponentArray : public IComponentArray {

	public:
		void insertData(uint32_t entity, T component)
		{
			assert(m_componentArray.find(entity) == m_componentArray.end() && "Adding component which already exists to entity");
			m_componentArray.emplace(entity, component);
		}
		
		void deleteData(uint32_t entity)
		{
			m_componentArray.erase(entity);
		}

		T* getData(uint32_t entity)
		{
			if (m_componentArray.contains(entity)) {
				return &(m_componentArray.at(entity));
			} else {
				return nullptr;
			}
		}

		std::map<uint32_t, T> m_componentArray{};

	};

	class System {
	
	public:
		System(Scene* scene, std::set<size_t> requiredComponentHashes);
		~System() {}
		System(const System&) = delete;
		System& operator=(const System&) = delete;

		virtual void onUpdate(float ts) = 0;

		Scene* const m_scene;

		std::bitset<MAX_COMPONENTS> m_signature;
		std::set<uint32_t> m_entities{}; // entities that contain the required components

	};

}
