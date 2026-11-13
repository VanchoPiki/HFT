#include "net/ws_client.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <optional>

#include "net/asio_sync.hpp"
#include "net/url.hpp"

namespace hft::net {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

using PlainWs = websocket::stream<beast::tcp_stream>;
using TlsWs = websocket::stream<beast::ssl_stream<beast::tcp_stream>>;

websocket::stream_base::timeout client_timeouts() {
    websocket::stream_base::timeout t{};
    t.handshake_timeout = std::chrono::seconds(30);
    t.idle_timeout = std::chrono::seconds(20);  // ping after 10s of silence, fail after 20s
    t.keep_alive_pings = true;
    return t;
}

}  // namespace

struct WsClient::Impl {
    explicit Impl(const std::string& u) : url(Url::parse(u)) {
        ssl_ctx.set_default_verify_paths();
        ssl_ctx.set_verify_mode(ssl::verify_peer);
    }

    Url url;
    asio::io_context ioc;
    ssl::context ssl_ctx{ssl::context::tls_client};
    std::optional<PlainWs> plain;
    std::optional<TlsWs> tls;
    beast::flat_buffer buffer;

    template <typename Ws>
    asio::awaitable<void> handshake(Ws& ws) {
        beast::get_lowest_layer(ws).expires_never();  // websocket timeouts take over
        ws.set_option(client_timeouts());
        ws.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) { req.set(beast::http::field::user_agent, "hft-bot"); }));
        ws.read_message_max(64 * 1024 * 1024);
        co_await ws.async_handshake(url.host, url.target, asio::use_awaitable);
    }

    asio::awaitable<void> connect() {
        plain.reset();
        tls.reset();
        buffer.clear();
        tcp::resolver resolver(ioc);
        const auto endpoints =
            co_await resolver.async_resolve(url.host, url.port, asio::use_awaitable);
        if (url.tls()) {
            tls.emplace(ioc, ssl_ctx);
            auto& ssl_stream = tls->next_layer();
            if (!SSL_set_tlsext_host_name(ssl_stream.native_handle(), url.host.c_str())) {
                throw std::runtime_error("ws: failed to set SNI host name");
            }
            ssl_stream.set_verify_callback(ssl::host_name_verification(url.host));
            auto& layer = beast::get_lowest_layer(*tls);
            layer.expires_after(std::chrono::seconds(30));
            co_await layer.async_connect(endpoints, asio::use_awaitable);
            layer.socket().set_option(tcp::no_delay(true));
            co_await ssl_stream.async_handshake(ssl::stream_base::client, asio::use_awaitable);
            co_await handshake(*tls);
        } else {
            plain.emplace(ioc);
            auto& layer = beast::get_lowest_layer(*plain);
            layer.expires_after(std::chrono::seconds(30));
            co_await layer.async_connect(endpoints, asio::use_awaitable);
            layer.socket().set_option(tcp::no_delay(true));
            co_await handshake(*plain);
        }
    }

    asio::awaitable<void> send(std::string text) {
        if (tls) {
            co_await tls->async_write(asio::buffer(text), asio::use_awaitable);
        } else {
            co_await plain->async_write(asio::buffer(text), asio::use_awaitable);
        }
    }

    asio::awaitable<std::string> read() {
        buffer.clear();
        if (tls) {
            co_await tls->async_read(buffer, asio::use_awaitable);
        } else {
            co_await plain->async_read(buffer, asio::use_awaitable);
        }
        co_return beast::buffers_to_string(buffer.data());
    }

    void require_connected() const {
        if (!plain && !tls) {
            throw std::logic_error("ws: not connected");
        }
    }

    // One background read at a time; its result is parked here until read_for() takes it.
    bool reading = false;
    bool read_ready = false;
    std::exception_ptr read_error;
    std::string read_message;

    std::optional<std::string> read_for(std::chrono::microseconds timeout) {
        if (!reading && !read_ready) {
            reading = true;
            asio::co_spawn(ioc, read(), [this](std::exception_ptr e, std::string message) {
                read_error = e;
                read_message = std::move(message);
                read_ready = true;
                reading = false;
            });
        }
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!read_ready) {
            if (ioc.run_one_until(deadline) == 0) {
                if (ioc.stopped()) {
                    ioc.restart();
                }
                if (std::chrono::steady_clock::now() >= deadline) {
                    return std::nullopt;
                }
            }
        }
        read_ready = false;
        if (read_error) {
            std::rethrow_exception(std::exchange(read_error, nullptr));
        }
        return std::move(read_message);
    }
};

WsClient::WsClient(const std::string& url) : impl_(std::make_unique<Impl>(url)) {}

WsClient::~WsClient() = default;

void WsClient::connect() { detail::run_until_done(impl_->ioc, impl_->connect()); }

void WsClient::send(std::string_view text) {
    impl_->require_connected();
    detail::run_until_done(impl_->ioc, impl_->send(std::string(text)));
}

std::string WsClient::read() {
    for (;;) {
        if (auto message = read_for(std::chrono::seconds(1))) {
            return std::move(*message);
        }
    }
}

std::optional<std::string> WsClient::read_for(std::chrono::microseconds timeout) {
    impl_->require_connected();
    return impl_->read_for(timeout);
}

}  // namespace hft::net
