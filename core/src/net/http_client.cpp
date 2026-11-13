#include "net/http_client.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

#include <optional>

#include "net/asio_sync.hpp"
#include "net/url.hpp"

namespace hft::net {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

}  // namespace

HttpStatusError::HttpStatusError(int s, const std::string& body)
    : std::runtime_error("http status " + std::to_string(s) + ": " + body.substr(0, 300)),
      status(s) {}

struct HttpClient::Impl {
    Impl(const std::string& u, std::chrono::milliseconds t) : url(Url::parse(u)), timeout(t) {
        ssl_ctx.set_default_verify_paths();
        ssl_ctx.set_verify_mode(ssl::verify_peer);
    }

    Url url;
    std::chrono::milliseconds timeout;
    asio::io_context ioc;
    ssl::context ssl_ctx{ssl::context::tls_client};
    std::optional<beast::tcp_stream> plain;
    std::optional<beast::ssl_stream<beast::tcp_stream>> tls;
    beast::flat_buffer buffer;

    bool connected() const { return plain.has_value() || tls.has_value(); }

    void disconnect() {
        plain.reset();
        tls.reset();
        buffer.clear();
    }

    asio::awaitable<void> connect() {
        tcp::resolver resolver(ioc);
        const auto endpoints =
            co_await resolver.async_resolve(url.host, url.port, asio::use_awaitable);
        if (url.tls()) {
            tls.emplace(ioc, ssl_ctx);
            if (!SSL_set_tlsext_host_name(tls->native_handle(), url.host.c_str())) {
                throw std::runtime_error("http: failed to set SNI host name");
            }
            tls->set_verify_callback(ssl::host_name_verification(url.host));
            auto& layer = beast::get_lowest_layer(*tls);
            layer.expires_after(timeout);
            co_await layer.async_connect(endpoints, asio::use_awaitable);
            layer.socket().set_option(tcp::no_delay(true));
            layer.expires_after(timeout);
            co_await tls->async_handshake(ssl::stream_base::client, asio::use_awaitable);
        } else {
            plain.emplace(ioc);
            plain->expires_after(timeout);
            co_await plain->async_connect(endpoints, asio::use_awaitable);
            plain->socket().set_option(tcp::no_delay(true));
        }
    }

    template <typename Stream>
    asio::awaitable<http::response<http::string_body>> exchange(
        Stream& stream, http::request<http::string_body>& req) {
        beast::get_lowest_layer(stream).expires_after(timeout);
        co_await http::async_write(stream, req, asio::use_awaitable);
        http::response_parser<http::string_body> parser;
        parser.body_limit(64 * 1024 * 1024);
        co_await http::async_read(stream, buffer, parser, asio::use_awaitable);
        co_return parser.release();
    }

    asio::awaitable<std::string> request(http::verb verb, std::string target, std::string body) {
        if (!connected()) {
            co_await connect();
        }
        http::request<http::string_body> req{verb, target, 11};
        req.set(http::field::host, url.host);
        req.set(http::field::user_agent, "hft-bot");
        if (verb == http::verb::post) {
            req.set(http::field::content_type, "application/json");
        }
        req.keep_alive(true);
        req.body() = std::move(body);
        req.prepare_payload();

        auto res = tls ? co_await exchange(*tls, req) : co_await exchange(*plain, req);
        if (!res.keep_alive()) {
            disconnect();
        }
        if (res.result_int() != 200) {
            throw HttpStatusError(static_cast<int>(res.result_int()), res.body());
        }
        co_return std::move(res.body());
    }

    std::string send(http::verb verb, std::string_view target, std::string_view body) {
        // A kept-alive connection may have been closed by the server in the meantime:
        // retry once on a fresh connection.
        for (int attempt = 0;; ++attempt) {
            const bool reused = connected();
            try {
                return detail::run_until_done(
                    ioc, request(verb, std::string(target), std::string(body)));
            } catch (const HttpStatusError&) {
                throw;
            } catch (const std::exception&) {
                disconnect();
                if (!reused || attempt > 0) {
                    throw;
                }
            }
        }
    }
};

HttpClient::HttpClient(const std::string& url, std::chrono::milliseconds timeout)
    : impl_(std::make_unique<Impl>(url, timeout)) {}

HttpClient::~HttpClient() = default;

std::string HttpClient::post_json(std::string_view body) {
    return impl_->send(http::verb::post, impl_->url.target, body);
}

std::string HttpClient::get(std::string_view target) {
    return impl_->send(http::verb::get, target, {});
}

}  // namespace hft::net
