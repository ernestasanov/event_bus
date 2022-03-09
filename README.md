# event_bus
MPMC (multi-producer multi-consumer) Event Bus implemented in C++17

Based on a lock-free single-producer single-consumer queue at https://github.com/cameron314/readerwriterqueue

Routing of events happens on a dedicated Event Bus thread.
Event Bus works as a common producer to multiple worker queues.
The events are pushed to the bus either guarded by a lock or with
another lock-free queue (in case of single-producer).

Event to data mapping is done at compile time