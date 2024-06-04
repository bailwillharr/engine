#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <queue>
#include <unordered_map>

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
    virtual void despatchEvents() = 0;
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
        m_event_queue.emplace(handler, event);
    }

    void despatchEvents() override
    {
        while (m_event_queue.empty() == false) {
            auto [handler, event] = m_event_queue.front();
            handler->onEvent(event);
            m_event_queue.pop();
        }
    }

   private:
    std::unordered_map<uint32_t, EventHandler<T>*> m_subscribers;

    struct QueuedEvent {
        EventHandler<T>* handler;
        T event;
    };
    std::queue<QueuedEvent> m_event_queue{};
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
        assert(m_event_queues.contains(hash) == false && "Registering an event queue more than once!");
        m_event_queues.emplace(hash, std::make_unique<EventQueue<T>>());
    }

    template <typename T>
    void subscribeToEventType(EventSubscriberKind kind, uint32_t id, EventHandler<T>* handler)
    {
        size_t hash = typeid(T).hash_code();
        assert(m_event_queues.contains(hash) && "Subscribing to event type that isn't registered!");
        EventQueue<T>* queue = dynamic_cast<EventQueue<T>*>(m_event_queues.at(hash).get());
        assert(queue != nullptr && "This cast should work?!! wot");
        queue->subscribe(kind, id, handler);
    }

    template <typename T>
    void queueEvent(EventSubscriberKind kind, uint32_t subscriber_id, T event)
    {
        size_t hash = typeid(T).hash_code();
        assert(m_event_queues.contains(hash) && "Subscribing to event type that isn't registered!");
        EventQueue<T>* queue = dynamic_cast<EventQueue<T>*>(m_event_queues.at(hash).get());
        assert(queue != nullptr && "This cast should work?!! wot");
        queue->queueEvent(kind, subscriber_id, event);
    }

    void despatchEvents()
    {
        for (auto& [hash, queue] : m_event_queues) {
            queue->despatchEvents();
        }
    }

   private:
    std::unordered_map<size_t, std::unique_ptr<IEventQueue>> m_event_queues{};
};

} // namespace engine