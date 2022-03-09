#ifndef __EVENT_BUS__
#define __EVENT_BUS__

#include <any>
#include <unordered_map>
#include <memory>

#include "event_producer.h"

namespace event_bus {
    template<class EventBus>
    class EventQueue;

    // Event is supposed to be an enumeration (or any integral type)
    // Helper class to associate event data with events
    template<typename Event, Event>
    struct EventData {
        using data_type = std::void_t<Event>;
    };

    // Required at compile time to define a storage for all event handlers
    template<typename Event>
    constexpr static size_t EventCount = 0;

    // Handlers are stored per thread, each handler is associated with a unique subscriber
    // Subscribers are typically objects, so they are type-erased to identifiers by void_t
    // Handlers are type-erased with std::any
    using EventHandler = std::unordered_map<ThreadToken, std::unordered_map<void *, std::any>>;

    // Event bus handles routing events between threads - any thread can post an event with associated
    // data, and it'll be passed (no copy - data is available by constant shared access) to the subscribed
    // worker threads
    // The concurrency is controlled by having a single Event Bus thread that modifies the storage
    template<typename Event, class EventProducer = EventProducerLock>
    class EventBus final {
        friend class EventQueue<EventBus>;

    public:
        using EventType = Event;

        // Access is shared by subscribers - the data is destroyed when the last handler frees it
        template<Event event>
        using EventDataAccessType = std::shared_ptr<const typename EventData<Event, event>::data_type>;

        ~EventBus() {
            Stop();

            // Must join if it has ended by itself
            if (m_eventBusThread.joinable()) {
                m_eventBusThread.join();
            }
        }

        // Submit event for routing to the handlers
        template<Event event>
        void FireEvent(EventDataAccessType<event> data) {
            PushEventItem([this, d = std::move(data)]() mutable {
                NotifyHandlers<event>(std::move(d));
            });
        }

        // Utility method to post a single-fire callback to some thread
        void PostCallback(ThreadToken threadToken, std::function<void()> callback) {
            PushEventItem([this, threadToken, callback = std::move(callback)]() mutable {
                NotifyThread(threadToken, std::move(callback));
            });
        }

        // Starts the event bus worker thread
        void Start() {
            if (m_eventBusThread.joinable()) {
                Stop();
                m_eventBusThread.join();
            }
            m_eventBusThread = std::thread(&EventBus::ProcessEventQueue, this);
        }

        void Stop() {
            m_eventProducer.Push([this] { m_keepRunning = false; });
        }

    private:

        // Register a worker thread with a callback queue for event processing
        void Register(ThreadToken threadToken, std::shared_ptr<WorkerQueue> workerQueue) {
            PushEventItem([this, threadToken, queue = std::move(workerQueue)]() mutable {
                if (m_workerQueues.find(threadToken) != m_workerQueues.end()) {
                    queue->try_enqueue([] { throw std::runtime_error("Duplicated worker queue on the same thread"); });
                    return;
                }
                m_workerQueues[threadToken] = std::move(queue);
            });
        }

        // Unregister worker thread
        void Unregister(ThreadToken threadToken) {
            PushEventItem([this, threadToken] {
                m_workerQueues.erase(threadToken);
                for (auto &eventHandler: m_eventHandlers) {
                    eventHandler.erase(threadToken);
                }
            });
        }

        // Add an event subscriber with a function handler on a thread
        template<Event event>
        void Subscribe(
                ThreadToken threadToken,
                void *subscriber,
                std::function<void(EventDataAccessType<event>)> handler
        ) {
            PushEventItem([this, threadToken, subscriber, h = std::move(handler)]() mutable {
                m_eventHandlers[static_cast<int>(event)][threadToken][subscriber] = std::any(std::move(h));
            });
        }

        // Remove subscriber
        template<Event event>
        void Unsubscribe(ThreadToken threadToken, void *subscriber) {
            PushEventItem([this, threadToken, subscriber] {
                auto &eventHandler = m_eventHandlers[static_cast<int>(event)];
                auto &threadHandler = eventHandler[threadToken];
                threadHandler.erase(subscriber);
                if (threadHandler.empty()) {
                    eventHandler.erase(threadToken);
                }
            });
        }

        // Post a callback to be executed on the event bus thread
        void PushEventItem(std::function<void()> callback) {
            m_eventProducer.Push(std::move(callback));
        }

        // The actual routing happens here on event bus thread:
        // the event data is packed (for each subscriber)
        // into a lambda that's then queued for execution on worker queue
        template<Event event>
        void NotifyHandlers(EventDataAccessType<event> data) const {
            using HandlerFunc = std::function<void(EventDataAccessType<event>)>;
            for (const auto& [threadToken, handlers]: m_eventHandlers[static_cast<int>(event)]) {
                auto workerQueue = m_workerQueues.find(threadToken);
                if (workerQueue == m_workerQueues.end()) {
                    continue;
                }
                for (const auto& [subscriber, handlerAny]: handlers) {
                    auto handler = std::any_cast<HandlerFunc>(handlerAny);
                    workerQueue->second->try_enqueue([data, h = std::move(handler)]() mutable {
                        h(std::move(data));
                    });
                }
            }
        }

        // Add callback to callback queue (called from event bus thread)
        void NotifyThread(ThreadToken threadToken, std::function<void()> callback) {
            auto workerQueue = m_workerQueues.find(threadToken);
            if (workerQueue == m_workerQueues.end()) {
                return;
            }
            workerQueue->second->try_enqueue(std::move(callback));
        }

        // Event bus thread is just a loop of callbacks
        void ProcessEventQueue() {
            m_keepRunning = true;
            std::function<void()> f;
            while (m_keepRunning) {
                m_eventProducer.Process(f);
                f = nullptr;
            }
        }

        std::unordered_map<ThreadToken, std::shared_ptr<WorkerQueue>> m_workerQueues;
        std::array<EventHandler, EventCount<Event>> m_eventHandlers;
        std::thread m_eventBusThread;
        bool m_keepRunning = true;
        EventProducer m_eventProducer;
    };
}

#endif // __EVENT_BUS__