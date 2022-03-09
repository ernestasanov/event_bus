#ifndef __EVENT_QUEUE__
#define __EVENT_QUEUE__

#include "event_producer.h"

namespace event_bus {
    // Event queue is attached to a thread, holds a queue of callbacks to be executed on this thread
    template<class EventBus>
    class EventQueue final {
    public:
        using Event = typename EventBus::EventType;

        explicit EventQueue(const std::shared_ptr<EventBus> &eventBus) :
                m_eventBus(eventBus),
                m_queue(std::make_shared<WorkerQueue>()),
                m_threadToken(std::this_thread::get_id()), m_callFunc(nullptr) {
            eventBus->Register(m_threadToken, m_queue);
        }

        ~EventQueue() {
            if (auto eventBus = m_eventBus.lock()) {
                eventBus->Unregister(m_threadToken);
            }
        }

        // Call this to execute pending callbacks
        void HandleEvents() {
            while (m_queue->try_dequeue(m_callFunc)) {
                m_callFunc();
            }
            m_callFunc = nullptr;
        }

        ThreadToken GetToken() const {
            return m_threadToken;
        }

        template<Event event>
        void Subscribe(
                void *subscriber,
                std::function<void(std::shared_ptr<const typename EventData<Event, event>::data_type>)> handler
        ) {
            if (auto eventBus = m_eventBus.lock()) {
                eventBus->template Subscribe<event>(m_threadToken, subscriber, std::move(handler));
            }
        }

        template<Event event>
        void Unsubscribe(void *subscriber) {
            if (auto eventBus = m_eventBus.lock()) {
                eventBus->template Unsubscribe<event>(m_threadToken, subscriber);
            }
        }

        void PostCallback(std::function<void()> callback) {
            if (auto eventBus = m_eventBus.lock()) {
                eventBus->PostCallback(m_threadToken, std::move(callback));
            }
        }

    private:
        ThreadToken m_threadToken;
        std::weak_ptr<EventBus> m_eventBus;
        std::shared_ptr<WorkerQueue> m_queue;
        std::function<void()> m_callFunc;
    };
}

#endif //__EVENT_QUEUE__