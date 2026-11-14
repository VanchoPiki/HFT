#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "telemetry/bounded_queue.hpp"

namespace hft::telemetry {

class Publisher {
public:
    explicit Publisher(std::string endpoint, std::size_t capacity = 65536);
    ~Publisher();
    Publisher(const Publisher&) = delete;
    Publisher& operator=(const Publisher&) = delete;

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

}
