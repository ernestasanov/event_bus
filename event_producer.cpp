#include "event_producer.h"

namespace event_bus {
    void EventProducerLock::Push(std::function<void()> eventItem) {
        std::lock_guard lock(m_eventQueueMutex);
        m_eventQueue.push(std::move(eventItem));
        m_eventQueueCV.notify_one();
    }

    void EventProducerLock::Process(std::function<void()> &eventItem) {
        {
            std::unique_lock lock(m_eventQueueMutex);
            if (m_eventQueue.empty()) {
                m_eventQueueCV.wait(lock);
            }
            eventItem = std::move(m_eventQueue.front());
            m_eventQueue.pop();
        }
        eventItem();
    }

    void EventProducerLockFree::Push(std::function<void()> eventItem) {
        if (m_threadId == m_zeroId) {
            m_threadId = std::this_thread::get_id();
        } else if (m_threadId != std::this_thread::get_id()) {
            throw std::runtime_error("Lock-free event bus supports posting callbacks only from one thread");
        }
        m_eventQueue.try_enqueue(std::move(eventItem));
    }

    void EventProducerLockFree::Process(std::function<void()> &eventItem) {
        if (!m_eventQueue.try_dequeue(eventItem)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return;
        } else {
            eventItem();
        }
    }
}
