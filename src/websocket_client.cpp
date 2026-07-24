#include "websocket_client.hpp"

#include "order_book.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <openssl/ssl.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;

using tcp = net::ip::tcp;
using json = nlohmann::json;

namespace {

std::vector<PriceLevel> parse_levels(
    const json& message,
    const std::string& side_name
)
{
    if (!message.contains(side_name)) {
        throw std::runtime_error(
            "WebSocket book event is missing '" +
            side_name +
            "'."
        );
    }

    const json& levels = message.at(side_name);

    if (!levels.is_array()) {
        throw std::runtime_error(
            "'" + side_name +
            "' must be a JSON array."
        );
    }

    std::vector<PriceLevel> parsed_levels;
    parsed_levels.reserve(levels.size());

    for (const json& level : levels) {
        if (!level.is_object()) {
            throw std::runtime_error(
                "Each WebSocket order-book level "
                "must be an object."
            );
        }

        if (
            !level.contains("price") ||
            !level.contains("size")
        ) {
            throw std::runtime_error(
                "WebSocket order-book level is "
                "missing price or size."
            );
        }

        if (
            !level.at("price").is_string() ||
            !level.at("size").is_string()
        ) {
            throw std::runtime_error(
                "WebSocket price and size must "
                "be strings."
            );
        }

        parsed_levels.push_back(
            PriceLevel{
                OrderBook::price_to_ticks(
                    level.at("price")
                        .get<std::string>()
                ),
                OrderBook::quantity_to_fixed(
                    level.at("size")
                        .get<std::string>()
                )
            }
        );
    }

    return parsed_levels;
}

void print_live_book(const OrderBook& book)
{
    const auto best_bid = book.best_bid();
    const auto best_ask = book.best_ask();

    std::cout << "Live order book loaded\n";

    if (best_bid.has_value()) {
        std::cout
            << "Best bid: "
            << OrderBook::format_price(
                   best_bid->price_ticks,
                   3
               )
            << '\n';
    }
    else {
        std::cout << "Best bid: N/A\n";
    }

    if (best_ask.has_value()) {
        std::cout
            << "Best ask: "
            << OrderBook::format_price(
                   best_ask->price_ticks,
                   3
               )
            << '\n';
    }
    else {
        std::cout << "Best ask: N/A\n";
    }

    const auto spread = book.spread_ticks();

    if (spread.has_value()) {
        std::cout
            << "Spread:   "
            << OrderBook::format_price(
                   spread.value(),
                   3
               )
            << '\n';
    }
    else {
        std::cout << "Spread:   N/A\n";
    }

    const auto mid_price = book.mid_price_ticks();

    if (mid_price.has_value()) {
        std::cout
            << "Mid:      "
            << OrderBook::format_price(
                   mid_price.value(),
                   3
               )
            << '\n';
    }
    else {
        std::cout << "Mid:      N/A\n";
    }

    std::cout
        << "Bid levels: "
        << book.bids().size()
        << '\n';

    std::cout
        << "Ask levels: "
        << book.asks().size()
        << '\n';
}

} // namespace

std::string WebSocketClient::build_subscription_message(
    const std::string& token_id
)
{
    const json message = {
        {"assets_ids", {token_id}},
        {"type", "market"}
    };

    return message.dump();
}

void WebSocketClient::stream_market(
    const std::string& token_id
) const
{
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
        << '\n';

    const std::string subscription_message =
        build_subscription_message(token_id);

    ws.write(
        net::buffer(subscription_message)
    );

    std::cout
        << "Subscription sent\n";

    beast::flat_buffer buffer;

    ws.read(buffer);

    const std::string response =
        beast::buffers_to_string(
            buffer.data()
        );

    const json messages =
        json::parse(response);

    if (!messages.is_array()) {
        throw std::runtime_error(
            "Expected a WebSocket JSON array."
        );
    }

    if (messages.empty()) {
        throw std::runtime_error(
            "WebSocket response array is empty."
        );
    }

    const json& message = messages.front();

    if (!message.is_object()) {
        throw std::runtime_error(
            "Expected a WebSocket message object."
        );
    }

    if (
        !message.contains("event_type") ||
        !message.at("event_type").is_string()
    ) {
        throw std::runtime_error(
            "WebSocket message is missing event_type."
        );
    }

    const std::string event_type =
        message.at("event_type")
            .get<std::string>();

    if (event_type != "book") {
        throw std::runtime_error(
            "Expected a book event, received: " +
            event_type
        );
    }

    std::vector<PriceLevel> bids =
        parse_levels(message, "bids");

    std::vector<PriceLevel> asks =
        parse_levels(message, "asks");

    OrderBook book;

    book.replace_snapshot(
        std::move(bids),
        std::move(asks)
    );

    print_live_book(book);

    ws.close(
        websocket::close_code::normal
    );
}