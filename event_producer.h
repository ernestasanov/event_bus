#ifndef __EVENT_PRODUCER__
#define __EVENT_PRODUCER__

#include <mutex>
#include <queue>
#include <functional>
#include <thread>
#include <condition_variable>

#include "readerwriterqueue.h"

namespace event_bus {
    using ThreadToken = std::thread::id;
    // Single-producer single-consumer RW queue
    using WorkerQueue = moodycamel::ReaderWriterQueue<std::function<void()>>;

    template<typename Event, class EventProducer>
    class EventBus;

    // multi-producer case: if callbacks are pushed from multiple threads
    class EventProducerLock final {
    private:
        template<typename Event, class EventProducer>
        friend class EventBus;

        void Push(std::function<void()> callback);

        void Process(std::function<void()> &callback);

        std::mutex m_eventQueueMutex;
        std::condition_variable m_eventQueueCV;
        std::queue<std::function<void()>> m_eventQueue;
    };

// single-producer case: if callbacks are pushed from a single thread
    class EventProducerLockFree final {
    private:
        template<typename Event, class EventProducer>
        friend class EventBus;

        void Push(std::function<void()> callback);

        void Process(std::function<void()> &callback);

        const std::thread::id m_zeroId;
        std::thread::id m_threadId;
        WorkerQueue m_eventQueue;
    };
}

#endif // __EVENT_PRODUCER__