#include "telemetry/publisher.hpp"

#include <zmq.hpp>

#include "util/log.hpp"

namespace hft::telemetry {

Publisher::Publisher(std::string endpoint, std::size_t capacity)
    : endpoint_(std::move(endpoint)), queue_(capacity) {
    if (!endpoint_.empty()) {
        thread_ = std::thread([this] { run(); });
    }
}

Publisher::~Publisher() {
    queue_.close();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Publisher::publish(std::string event) {
    if (endpoint_.empty()) {
        return;
    }
    if (!queue_.try_push(std::move(event))) {
        ++dropped_;
    }
}

void Publisher::run() {
    try {
        zmq::context_t context(1);
        zmq::socket_t socket(context, zmq::socket_type::pub);
        socket.set(zmq::sockopt::sndhwm, 100000);
        socket.set(zmq::sockopt::linger, 500);  // flush on shutdown, but do not hang
        socket.bind(endpoint_);
        log::info("telemetry: publishing on {}", endpoint_);

        std::string event;
        while (!queue_.closed_and_empty()) {
            if (!queue_.pop(event, std::chrono::milliseconds(200))) {
                continue;
            }
            if (socket.send(zmq::buffer(event), zmq::send_flags::dontwait)) {
                ++published_;
            } else {
                ++dropped_;
            }
        }
    } catch (const std::exception& e) {
        log::error("telemetry: {} - events will be dropped", e.what());
    }
}

}  // namespace hft::telemetry
