#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>

namespace hft::telemetry {

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}

    bool try_push(T value) {
        {
            const std::lock_guard lock(mutex_);
            if (closed_ || items_.size() >= capacity_) {
                return false;
            }
            items_.push_back(std::move(value));
        }
        ready_.notify_one();
        return true;
    }

    bool pop(T& out, std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        if (!ready_.wait_for(lock, timeout, [&] { return closed_ || !items_.empty(); }) ||
            items_.empty()) {
            return false;
        }
        out = std::move(items_.front());
        items_.pop_front();
        return true;
    }

    void close() {
        {
            const std::lock_guard lock(mutex_);
            closed_ = true;
        }
        ready_.notify_all();
    }

    bool closed_and_empty() const {
        const std::lock_guard lock(mutex_);
        return closed_ && items_.empty();
    }

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<T> items_;
    bool closed_ = false;
};

}
