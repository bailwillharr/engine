#pragma once

#include <queue>
#include <unordered_map>
#include <memory>
#include <cstdlib>
#include <cassert>
#include <typeinfo>

namespace engine {

	enum class EventSubscriberKind {
		ENTITY,
	};

	// Event handler base-class
	template <typename T>
	class EventHandler {
	public:
		virtual void onEvent(T data) = 0;
	};

	// Event queue interface to allow for different type queues to be in the map
	class IEventQueue {
	public:
		virtual ~IEventQueue() {}
		virtual void dispatchEvents() = 0;
	};

	template <typename T>
	class EventQueue : public IEventQueue {
		// holds events of type T and subscribers to those events

	public:
		void subscribe(EventSubscriberKind kind, uint32_t id, EventHandler<T>* handler)
		{
			// For the time being, ignore kind (TODO)
			(void)kind;
			assert(m_subscribers.contains(id) == false && "subscribing to an event with ID that's already in use!");
			m_subscribers.emplace(id, handler);
		}

		void queueEvent(EventSubscriberKind kind, uint32_t id, T event)
		{
			// For the time being, ignore kind (TODO)
			(void)kind;
			assert(m_subscribers.contains(id) && "Attempt to submit event to non-existing subscriber!");
			EventHandler<T>* handler = m_subscribers.at(id);
			m_eventQueue.emplace(handler, event);
		}

		void dispatchEvents() override
		{
			while (m_eventQueue.empty() == false) {
				auto [handler, event] = m_eventQueue.front();
				handler->onEvent(event);
				m_eventQueue.pop();
			}
		}

	private:
		std::unordered_map<uint32_t, EventHandler<T>*> m_subscribers;

		struct QueuedEvent {

			QueuedEvent(EventHandler<T>* handler, T event) :
				handler(handler),
				event(event) {}

			EventHandler<T>* handler;
			T event;
		};
		std::queue<QueuedEvent> m_eventQueue{};

	};

	class EventSystem {
	
	public:
		EventSystem() {}
		EventSystem(const EventSystem&) = delete;
		EventSystem& operator=(const EventSystem&) = delete;
		~EventSystem() {}

		template <typename T>
		void registerEventType()
		{
			size_t hash = typeid(T).hash_code();
			assert(m_eventQueues.contains(hash) == false && "Registering an event queue more than once!");
			m_eventQueues.emplace(hash, std::make_unique<EventQueue<T>>());
		}

		template <typename T>
		void subscribeToEventType(EventSubscriberKind kind, uint32_t id, EventHandler<T>* handler)
		{
			size_t hash = typeid(T).hash_code();
			assert(m_eventQueues.contains(hash) && "Subscribing to event type that isn't registered!");
			EventQueue<T>* queue = dynamic_cast<EventQueue<T>*>(m_eventQueues.at(hash).get());
			assert(queue != nullptr && "This cast should work?!! wot");
			queue->subscribe(kind, id, handler);
		}

		template <typename T>
		void queueEvent(EventSubscriberKind kind, uint32_t subscriberID, T event)
		{
			size_t hash = typeid(T).hash_code();
			assert(m_eventQueues.contains(hash) && "Subscribing to event type that isn't registered!");
			EventQueue<T>* queue = dynamic_cast<EventQueue<T>*>(m_eventQueues.at(hash).get());
			assert(queue != nullptr && "This cast should work?!! wot");
			queue->queueEvent(kind, subscriberID, event);
		}

		void dispatchEvents()
		{
			for (auto& [hash, queue] : m_eventQueues) {
				queue->dispatchEvents();
			}
		}

	private:
		std::unordered_map<size_t, std::unique_ptr<IEventQueue>> m_eventQueues{};

	};

}
