#pragma once

#include <format>
#include <string_view>
#include <utility>

namespace hft::log {

enum class Level { debug = 0, info = 1, warn = 2, error = 3 };

void set_level(Level level);
bool enabled(Level level);
// Thread-safe: "HH:MM:SS.mmm LEVEL message" to stderr.
void write(Level level, std::string_view message);

template <typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) {
    if (enabled(Level::debug)) write(Level::debug, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
    if (enabled(Level::info)) write(Level::info, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) {
    if (enabled(Level::warn)) write(Level::warn, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::error, std::format(fmt, std::forward<Args>(args)...));
}

}  // namespace hft::log
