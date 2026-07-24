#include "websocket_client.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <openssl/ssl.h>

#include <iostream>
#include <stdexcept>
#include <string>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;

using tcp = net::ip::tcp;

void WebSocketClient::stream_market(
    const std::string& token_id
) const
{
    (void)token_id;

    constexpr char host[] =
        "ws-subscriptions-clob.polymarket.com";

    constexpr char port[] = "443";

    constexpr char target[] = "/ws/market";

    net::io_context io_context;

    ssl::context ssl_context(
        ssl::context::tls_client
    );

    ssl_context.set_default_verify_paths();

    tcp::resolver resolver(io_context);

    websocket::stream<
        beast::ssl_stream<
            beast::tcp_stream
        >
    > ws(
        io_context,
        ssl_context
    );

    if (!SSL_set_tlsext_host_name(
            ws.next_layer().native_handle(),
            host
        )) {
        throw std::runtime_error(
            "Failed to set TLS hostname."
        );
    }

    const auto endpoints =
        resolver.resolve(host, port);

    beast::get_lowest_layer(ws)
        .connect(endpoints);

    ws.next_layer().handshake(
        ssl::stream_base::client
    );

    ws.handshake(host, target);

    std::cout
        << "Connected to "
        << host
        << std::endl;

    ws.close(
        websocket::close_code::normal
    );
}