#include "util/log.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace hft::log {
namespace {

std::atomic<Level> g_level{Level::info};
std::mutex g_mutex;

constexpr const char* kNames[] = {"DEBUG", "INFO ", "WARN ", "ERROR"};

}

void set_level(Level level) { g_level = level; }

bool enabled(Level level) { return level >= g_level.load(std::memory_order_relaxed); }

void write(Level level, std::string_view message) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm tm{};
    localtime_r(&t, &tm);

    const std::lock_guard lock(g_mutex);
    std::fprintf(stderr, "%02d:%02d:%02d.%03d %s %.*s\n", tm.tm_hour, tm.tm_min, tm.tm_sec,
                 static_cast<int>(ms), kNames[static_cast<int>(level)],
                 static_cast<int>(message.size()), message.data());
}

}
