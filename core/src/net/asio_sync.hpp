#pragma once

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

}
