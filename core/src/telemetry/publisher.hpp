#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "telemetry/bounded_queue.hpp"

namespace hft::telemetry {

// Ships JSON events to the Java analytics service over a ZeroMQ PUB socket from its own
// thread. Fire-and-forget: nothing on the trading path waits for Java (or for anyone
// to be subscribed at all).
class Publisher {
public:
    // Binds e.g. "tcp://127.0.0.1:5555". An empty endpoint disables publishing.
    explicit Publisher(std::string endpoint, std::size_t capacity = 65536);
    ~Publisher();
    Publisher(const Publisher&) = delete;
    Publisher& operator=(const Publisher&) = delete;

    // Never blocks; the event is dropped (and counted) if the queue is full.
    void publish(std::string event);

    std::uint64_t published() const { return published_; }
    std::uint64_t dropped() const { return dropped_; }

private:
    void run();

    std::string endpoint_;
    BoundedQueue<std::string> queue_;
    std::atomic<std::uint64_t> published_{0};
    std::atomic<std::uint64_t> dropped_{0};
    std::thread thread_;
};

}  // namespace hft::telemetry
