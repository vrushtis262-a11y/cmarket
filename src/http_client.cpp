#include "http_client.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

#include <openssl/ssl.h>

#include <stdexcept>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;

using tcp = net::ip::tcp;

std::string HttpClient::get(
    const std::string& host,
    const std::string& target,
    const std::string& port
) const
{
    net::io_context io_context;

    ssl::context ssl_context(ssl::context::tls_client);
    ssl_context.set_default_verify_paths();

    tcp::resolver resolver(io_context);
    beast::ssl_stream<beast::tcp_stream> stream(
        io_context,
        ssl_context
    );

    if (!SSL_set_tlsext_host_name(
            stream.native_handle(),
            host.c_str()
        )) {
        throw std::runtime_error("Failed to set TLS hostname.");
    }

    const auto endpoints = resolver.resolve(host, port);

    beast::get_lowest_layer(stream).connect(endpoints);

    stream.handshake(ssl::stream_base::client);

    http::request<http::empty_body> request{
        http::verb::get,
        target,
        11
    };

    request.set(http::field::host, host);
    request.set(http::field::user_agent, "cmarket/0.1.0");
    request.set(http::field::accept, "application/json");

    http::write(stream, request);

    beast::flat_buffer buffer;
    http::response<http::string_body> response;

    http::read(stream, buffer, response);

    if (response.result_int() < 200 ||
        response.result_int() >= 300) {
        throw std::runtime_error(
            "HTTP request failed with status " +
            std::to_string(response.result_int())
        );
    }

    beast::error_code shutdown_error;
    stream.shutdown(shutdown_error);

    if (shutdown_error == net::error::eof ||
        shutdown_error == ssl::error::stream_truncated) {
        shutdown_error = {};
    }

    if (shutdown_error) {
        throw beast::system_error(shutdown_error);
    }

    return response.body();
}