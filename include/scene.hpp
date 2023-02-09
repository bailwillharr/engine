#pragma once

#include "log.hpp"

#include "ecs_system.hpp"
#include "event_system.hpp"

#include <map>
#include <cstdint>
#include <typeinfo>
#include <assert.h>

namespace engine {

	class Application;

	class Scene {
	
	public:
		Scene(Application* app);
		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;
		~Scene();

		void update(float ts);

		Application* app() { return m_app; }

		EventSystem* events() { return m_eventSystem.get(); }

		/* ecs stuff */

		uint32_t createEntity(const std::string& tag, uint32_t parent = 0);
		
		uint32_t getEntity(const std::string& tag, uint32_t parent = 0);

		size_t getComponentSignaturePosition(size_t hash);
		
		template <typename T>
		void registerComponent()
		{
			size_t hash = typeid(T).hash_code();
			assert(m_componentArrays.contains(hash) == false && "Registering component type more than once.");
			m_componentArrays.emplace(hash, std::make_unique<ComponentArray<T>>());

			size_t componentSignaturePosition = m_nextSignaturePosition++;
			assert(componentSignaturePosition < MAX_COMPONENTS && "Registering too many components!");
			assert(m_componentSignaturePositions.contains(hash) == false);
			m_componentSignaturePositions.emplace(hash, componentSignaturePosition);
		}

		template <typename T>
		T* getComponent(uint32_t entity)
		{
			auto array = getComponentArray<T>();
			return array->getData(entity);
		}

		template <typename T>
		T* addComponent(uint32_t entity)
		{
			size_t hash = typeid(T).hash_code();

			auto array = getComponentArray<T>();
			array->insertData(entity, T{}); // errors if entity already exists in array

			// set the component bit for this entity
			size_t componentSignaturePosition = m_componentSignaturePositions.at(hash);
			auto& signatureRef = m_signatures.at(entity);
			signatureRef.set(componentSignaturePosition);

			for (auto& [systemName, system] : m_systems)
			{
				if (system->m_entities.contains(entity)) continue;
				if ((system->m_signature & signatureRef) == system->m_signature) {
					system->m_entities.insert(entity);
					system->onComponentInsert(entity);
				}
			}

			return array->getData(entity);
		}

		template <typename T>
		void registerSystem()
		{
			size_t hash = typeid(T).hash_code();
			assert(m_systems.find(hash) == m_systems.end() && "Registering system more than once.");
			m_systems.emplace(hash, std::make_unique<T>(this));
		}

		template <typename T>
		T* getSystem()
		{
			size_t hash = typeid(T).hash_code();
			auto it = m_systems.find(hash);
			if (it == m_systems.end()) {
				throw std::runtime_error("Cannot find ecs system.");
			}
			auto ptr = it->second.get();
			auto castedPtr = dynamic_cast<T*>(ptr);
			assert(castedPtr != nullptr);
			return castedPtr;
		}

	private:
		Application* const m_app;
		uint32_t m_nextEntityID = 1000;

		/* ecs stuff */

		size_t m_nextSignaturePosition = 0;
		// maps component hashes to signature positions
		std::map<size_t, size_t> m_componentSignaturePositions{};
		// maps entity ids to their signatures
		std::map<uint32_t, std::bitset<MAX_COMPONENTS>> m_signatures{};
		// maps component hashes to their arrays
		std::map<size_t, std::unique_ptr<IComponentArray>> m_componentArrays{};
		// maps system hashes to their class instantiations
		std::map<size_t, std::unique_ptr<System>> m_systems{};

		template <typename T>
		ComponentArray<T>* getComponentArray()
		{
			size_t hash = typeid(T).hash_code();
			auto it = m_componentArrays.find(hash);
			if (it == m_componentArrays.end()) {
				throw std::runtime_error("Cannot find component array.");
			}
			auto ptr = it->second.get();
			auto castedPtr = dynamic_cast<ComponentArray<T>*>(ptr);
			assert(castedPtr != nullptr);
			return castedPtr;
		}

		std::unique_ptr<EventSystem> m_eventSystem{};

	};

}
