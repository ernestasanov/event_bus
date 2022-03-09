#include "event_bus.h"
#include "event_queue.h"

#include <iostream>

enum MyEvent {
    Start,
    Stop,
    SENTINEL // Just for counting real events
};

template<>
constexpr auto event_bus::EventCount<MyEvent> = MyEvent::SENTINEL;

template<>
struct event_bus::EventData<MyEvent, MyEvent::Start> {
    using data_type = int;
};

template<>
struct event_bus::EventData<MyEvent, MyEvent::Stop> {
    using data_type = float;
};

using EventBus = event_bus::EventBus<MyEvent>;
using EventQueue = event_bus::EventQueue<EventBus>;

int main() {
    std::atomic_bool finished = false;
    // Event bus is common
    auto eventBus = std::make_shared<EventBus>();
    event_bus::ThreadToken worker1Token;
    auto worker = std::thread([&eventBus, &finished, &worker1Token]{
        // Event queues are per thread
        EventQueue eventQueue { eventBus };
        worker1Token = eventQueue.GetToken();
        eventQueue.Subscribe<MyEvent::Start>(&eventQueue, [&eventBus](auto eventData) {
            // event data is std::shared_ptr<const int>
            // this lambda is called in worker thread 1
            auto eventParam = *eventData; // int
            std::cout << "Worker 1 start event " << eventParam << std::endl;
            eventBus->FireEvent<MyEvent::Stop>(std::make_shared<float>(5.0f));
        });
        while (!finished) {
            // event queue callbacks are called in this loop
            eventQueue.HandleEvents();
        }
        eventQueue.Unsubscribe<MyEvent::Start>(&eventQueue); // Not required
    });
    auto worker2 = std::thread([&eventBus, &finished]{
        EventQueue eventQueue { eventBus };
        eventQueue.Subscribe<MyEvent::Start>(&eventQueue, [](auto eventData) {
            // this lambda is called in worker thread 2
            auto eventParam = *eventData; // int
            std::cout << "Worker 2 start event " << eventParam << std::endl;
        });
        eventQueue.Subscribe<MyEvent::Stop>(&eventQueue, [](auto eventData) {
            auto eventParam = *eventData; // float
            std::cout << "Worker 2 stop event " << eventParam << std::endl;
        });
        while (!finished) {
            eventQueue.HandleEvents();
        }
        eventQueue.Unsubscribe<MyEvent::Start>(&eventQueue); // Not required
        eventQueue.Unsubscribe<MyEvent::Stop>(&eventQueue); // Not required
    });
    eventBus->Start();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // Post event for all subscribers to consume
    eventBus->FireEvent<MyEvent::Start>(std::make_shared<int>(1));
    std::this_thread::sleep_for(std::chrono::seconds(5));
    finished = true;
    eventBus->Stop();
    worker.join();
    worker2.join();
}