#ifndef ENGINE_INCLUDE_EVENT_SYSTEM_H_
#define ENGINE_INCLUDE_EVENT_SYSTEM_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <queue>
#include <unordered_map>

namespace engine {

enum class EventSubscriberKind {
  kEntity,
};

// Event handler base-class
template <typename T>
class EventHandler {
 public:
  virtual void OnEvent(T data) = 0;
};

// Event queue interface to allow for different type queues to be in the map
class IEventQueue {
 public:
  virtual ~IEventQueue() {}
  virtual void DespatchEvents() = 0;
};

template <typename T>
class EventQueue : public IEventQueue {
  // holds events of type T and subscribers to those events

 public:
  void Subscribe(EventSubscriberKind kind, uint32_t id,
                 EventHandler<T>* handler) {
    // For the time being, ignore kind (TODO)
    (void)kind;
    assert(subscribers_.contains(id) == false &&
           "subscribing to an event with ID that's already in use!");
    subscribers_.emplace(id, handler);
  }

  void QueueEvent(EventSubscriberKind kind, uint32_t id, T event) {
    // For the time being, ignore kind (TODO)
    (void)kind;
    assert(subscribers_.contains(id) &&
           "Attempt to submit event to non-existing subscriber!");
    EventHandler<T>* handler = subscribers_.at(id);
    event_queue_.emplace(handler, event);
  }

  void DespatchEvents() override {
    while (event_queue_.empty() == false) {
      auto [handler, event] = event_queue_.front();
      handler->OnEvent(event);
      event_queue_.pop();
    }
  }

 private:
  std::unordered_map<uint32_t, EventHandler<T>*> subscribers_;

  struct QueuedEvent {
    QueuedEvent(EventHandler<T>* handler, T event)
        : handler(handler), event(event) {}

    EventHandler<T>* handler;
    T event;
  };
  std::queue<QueuedEvent> event_queue_{};
};

class EventSystem {
 public:
  EventSystem() {}
  EventSystem(const EventSystem&) = delete;
  EventSystem& operator=(const EventSystem&) = delete;
  ~EventSystem() {}

  template <typename T>
  void RegisterEventType() {
    size_t hash = typeid(T).hash_code();
    assert(event_queues_.contains(hash) == false &&
           "Registering an event queue more than once!");
    event_queues_.emplace(hash, std::make_unique<EventQueue<T>>());
  }

  template <typename T>
  void SubscribeToEventType(EventSubscriberKind kind, uint32_t id,
                            EventHandler<T>* handler) {
    size_t hash = typeid(T).hash_code();
    assert(event_queues_.contains(hash) &&
           "Subscribing to event type that isn't registered!");
    EventQueue<T>* queue =
        dynamic_cast<EventQueue<T>*>(event_queues_.at(hash).get());
    assert(queue != nullptr && "This cast should work?!! wot");
    queue->Subscribe(kind, id, handler);
  }

  template <typename T>
  void QueueEvent(EventSubscriberKind kind, uint32_t subscriber_id, T event) {
    size_t hash = typeid(T).hash_code();
    assert(event_queues_.contains(hash) &&
           "Subscribing to event type that isn't registered!");
    EventQueue<T>* queue =
        dynamic_cast<EventQueue<T>*>(event_queues_.at(hash).get());
    assert(queue != nullptr && "This cast should work?!! wot");
    queue->QueueEvent(kind, subscriber_id, event);
  }

  void DespatchEvents() {
    for (auto& [hash, queue] : event_queues_) {
      queue->DespatchEvents();
    }
  }

 private:
  std::unordered_map<size_t, std::unique_ptr<IEventQueue>> event_queues_{};
};

}  // namespace engine

#endif