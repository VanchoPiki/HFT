#pragma once

// Internal helper: runs one Asio coroutine to completion on the calling thread.
// Async operations are used (instead of Beast's blocking API) because only they honour
// beast::tcp_stream timeouts and WebSocket keep-alive pings.

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>

#include <exception>
#include <optional>

namespace hft::net::detail {

template <typename T>
T run_until_done(boost::asio::io_context& ioc, boost::asio::awaitable<T> task) {
    std::optional<T> result;
    std::exception_ptr error;
    bool done = false;
    boost::asio::co_spawn(ioc, std::move(task), [&](std::exception_ptr e, T value) {
        error = e;
        if (!e) {
            result.emplace(std::move(value));
        }
        done = true;
    });
    // run_one instead of run: background timers (e.g. WebSocket pings) must not block us.
    while (!done) {
        if (ioc.run_one() == 0) {
            ioc.restart();
        }
    }
    if (error) {
        std::rethrow_exception(error);
    }
    return std::move(*result);
}

inline void run_until_done(boost::asio::io_context& ioc, boost::asio::awaitable<void> task) {
    std::exception_ptr error;
    bool done = false;
    boost::asio::co_spawn(ioc, std::move(task), [&](std::exception_ptr e) {
        error = e;
        done = true;
    });
    while (!done) {
        if (ioc.run_one() == 0) {
            ioc.restart();
        }
    }
    if (error) {
        std::rethrow_exception(error);
    }
}

}  // namespace hft::net::detail
