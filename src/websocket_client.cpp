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

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;

using tcp = net::ip::tcp;
using json = nlohmann::json;

namespace {

volatile std::sig_atomic_t shutdown_requested = 0;

void handle_shutdown_signal(int)
{
    shutdown_requested = 1;
}


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

constexpr std::size_t top_level_count = 10;

std::string format_quantity(std::int64_t quantity)
{
    const std::int64_t whole =
        quantity / OrderBook::ticks_per_unit;

    const std::int64_t fractional =
        quantity % OrderBook::ticks_per_unit;

    std::ostringstream output;
    output << whole;

    if (fractional != 0) {
        output
            << '.'
            << std::setw(6)
            << std::setfill('0')
            << fractional;

        std::string value = output.str();

        while (!value.empty() && value.back() == '0') {
            value.pop_back();
        }

        return value;
    }

    return output.str();
}

void print_levels(
    const std::vector<PriceLevel>& levels,
    std::size_t maximum_levels
)
{
    const std::size_t level_count =
        std::min(maximum_levels, levels.size());

    if (level_count == 0) {
        std::cout << "  No levels\n";
        return;
    }

    std::cout
        << "  "
        << std::left
        << std::setw(10)
        << "Price"
        << "Quantity\n";

    for (std::size_t index = 0; index < level_count; ++index) {
        const PriceLevel& level = levels.at(index);

        std::cout
            << "  "
            << std::left
            << std::setw(10)
            << OrderBook::format_price(
                   level.price_ticks,
                   3
               )
            << format_quantity(level.quantity)
            << '\n';
    }
}

void print_live_book(const OrderBook& book)
{
    const auto best_bid = book.best_bid();
    const auto best_ask = book.best_ask();

    std::cout
        << "=========== ORDER BOOK ===========\n"
        << "ASKS (best first)\n";

    print_levels(
        book.asks(),
        top_level_count
    );

    std::cout
        << "----------------------------------\n"
        << "BIDS (best first)\n";

    print_levels(
        book.bids(),
        top_level_count
    );

    std::cout
        << "----------------------------------\n";

    if (best_bid.has_value()) {
        std::cout
            << "Best bid:  "
            << OrderBook::format_price(
                   best_bid->price_ticks,
                   3
               )
            << '\n';
    }
    else {
        std::cout << "Best bid:  N/A\n";
    }

    if (best_ask.has_value()) {
        std::cout
            << "Best ask:  "
            << OrderBook::format_price(
                   best_ask->price_ticks,
                   3
               )
            << '\n';
    }
    else {
        std::cout << "Best ask:  N/A\n";
    }

    const auto spread = book.spread_ticks();

    if (spread.has_value()) {
        std::cout
            << "Spread:    "
            << OrderBook::format_price(
                   spread.value(),
                   3
               )
            << '\n';
    }
    else {
        std::cout << "Spread:    N/A\n";
    }

    const auto mid_price = book.mid_price_ticks();

    if (mid_price.has_value()) {
        std::cout
            << "Mid:       "
            << OrderBook::format_price(
                   mid_price.value(),
                   3
               )
            << '\n';
    }
    else {
        std::cout << "Mid:       N/A\n";
    }

    std::cout
        << "Bid depth: "
        << format_quantity(book.bid_depth())
        << '\n';

    std::cout
        << "Ask depth: "
        << format_quantity(book.ask_depth())
        << '\n';

    std::cout
        << "Bid levels: "
        << book.bids().size()
        << '\n';

    std::cout
        << "Ask levels: "
        << book.asks().size()
        << '\n';

    std::cout
        << "==================================\n";
}

void apply_price_change(
    const json& change,
    const std::string& token_id,
    OrderBook& book
)
{
    if (!change.is_object()) {
        throw std::runtime_error(
            "Each price change must be an object."
        );
    }

    if (
        !change.contains("asset_id") ||
        !change.contains("price") ||
        !change.contains("size") ||
        !change.contains("side")
    ) {
        throw std::runtime_error(
            "Price change is missing asset_id, "
            "price, size, or side."
        );
    }

    if (
        !change.at("asset_id").is_string() ||
        !change.at("price").is_string() ||
        !change.at("size").is_string() ||
        !change.at("side").is_string()
    ) {
        throw std::runtime_error(
            "Price change fields must be strings."
        );
    }

    const std::string asset_id =
        change.at("asset_id")
            .get<std::string>();

    if (asset_id != token_id) {
        return;
    }

    const std::int64_t price_ticks =
        OrderBook::price_to_ticks(
            change.at("price")
                .get<std::string>()
        );

    const std::int64_t quantity =
        OrderBook::quantity_to_fixed(
            change.at("size")
                .get<std::string>()
        );

    const std::string side =
        change.at("side")
            .get<std::string>();

    if (side == "BUY") {
        book.update_bid(
            price_ticks,
            quantity
        );
    }
    else if (side == "SELL") {
        book.update_ask(
            price_ticks,
            quantity
        );
    }
    else {
        throw std::runtime_error(
            "Unsupported price-change side: " +
            side
        );
    }

    std::cout
        << "Applied "
        << side
        << " update at "
        << OrderBook::format_price(
               price_ticks,
               3
           )
        << " with size "
        << change.at("size").get<std::string>()
        << '\n';
}

void process_price_change(
    const json& message,
    const std::string& token_id,
    OrderBook& book
)
{
    if (!message.contains("price_changes")) {
        throw std::runtime_error(
            "price_change event is missing "
            "price_changes."
        );
    }

    const json& changes =
        message.at("price_changes");

    if (!changes.is_array()) {
        throw std::runtime_error(
            "price_changes must be an array."
        );
    }

    for (const json& change : changes) {
        apply_price_change(
            change,
            token_id,
            book
        );
    }

    std::cout
        << "========================\n";

    print_live_book(book);
}

void process_message(
    const json& message,
    const std::string& token_id,
    OrderBook& book,
    bool& snapshot_received
)
{
    if (!message.is_object()) {
        std::cout
            << "Ignored non-object WebSocket message\n";
        return;
    }

    if (
        !message.contains("event_type") ||
        !message.at("event_type").is_string()
    ) {
        std::cout
            << "Received message without event_type\n";
        return;
    }

    const std::string event_type =
        message.at("event_type")
            .get<std::string>();

    std::cout
        << "\nReceived event: "
        << event_type
        << '\n';

    if (event_type == "book") {
        std::vector<PriceLevel> bids =
            parse_levels(message, "bids");

        std::vector<PriceLevel> asks =
            parse_levels(message, "asks");

        book.replace_snapshot(
            std::move(bids),
            std::move(asks)
        );

        std::cout
            << "========================\n";

        snapshot_received = true;

        print_live_book(book);
        return;
    }

    if (event_type == "price_change") {
        if (!snapshot_received) {
            std::cout
                << "Ignored price_change before initial "
                << "book snapshot.\n";
            return;
        }

        process_price_change(
            message,
            token_id,
            book
        );

        return;
    }
}

void process_payload(
    const json& payload,
    const std::string& token_id,
    OrderBook& book,
    bool& snapshot_received
)
{
    if (payload.is_array()) {
        for (const json& message : payload) {
            process_message(
                message,
                token_id,
                book,
                snapshot_received
            );
        }

        return;
    }

    if (payload.is_object()) {
        process_message(
            payload,
            token_id,
            book,
            snapshot_received
        );

        return;
    }

    std::cout
        << "Ignored unsupported WebSocket payload\n";
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

    constexpr int reconnect_delay_seconds = 3;

    shutdown_requested = 0;

    const auto previous_sigint_handler =
        std::signal(SIGINT, handle_shutdown_signal);

    const auto previous_sigterm_handler =
        std::signal(SIGTERM, handle_shutdown_signal);

    const std::string subscription_message =
        build_subscription_message(token_id);

    std::cout
        << "Press Ctrl+C to stop streaming.\n";

    while (shutdown_requested == 0) {
        try {
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

            ws.write(
                net::buffer(subscription_message)
            );

            std::cout
                << "Subscription sent\n";

            OrderBook book;
            bool snapshot_received = false;
            beast::flat_buffer buffer;

            while (shutdown_requested == 0) {
                buffer.consume(buffer.size());

                beast::error_code error;
                ws.read(buffer, error);

                if (shutdown_requested != 0) {
                    break;
                }

                if (error == websocket::error::closed) {
                    std::cerr
                        << "WebSocket connection closed "
                        << "by server.\n";
                    break;
                }

                if (error) {
                    std::cerr
                        << "WebSocket read error: "
                        << error.message()
                        << '\n';
                    break;
                }

                const std::string response =
                    beast::buffers_to_string(
                        buffer.data()
                    );

                try {
                    const json payload =
                        json::parse(response);

                    process_payload(
                        payload,
                        token_id,
                        book,
                        snapshot_received
                    );
                }
                catch (const json::exception& error) {
                    std::cerr
                        << "Ignored invalid JSON message: "
                        << error.what()
                        << '\n';
                }
                catch (const std::exception& error) {
                    std::cerr
                        << "Ignored invalid market event: "
                        << error.what()
                        << '\n';
                }
            }

            if (shutdown_requested != 0) {
                std::cout
                    << "\nShutdown requested. "
                    << "Closing WebSocket...\n";
            }

            if (ws.is_open()) {
                beast::error_code close_error;

                ws.close(
                    websocket::close_code::normal,
                    close_error
                );

                if (
                    close_error &&
                    close_error != websocket::error::closed
                ) {
                    std::cerr
                        << "WebSocket close warning: "
                        << close_error.message()
                        << '\n';
                }
            }
        }
        catch (const std::exception& error) {
            if (shutdown_requested == 0) {
                std::cerr
                    << "WebSocket connection error: "
                    << error.what()
                    << '\n';
            }
        }

        if (shutdown_requested != 0) {
            break;
        }

        std::cout
            << "Reconnecting in "
            << reconnect_delay_seconds
            << " seconds...\n";

        for (
            int second = 0;
            second < reconnect_delay_seconds &&
            shutdown_requested == 0;
            ++second
        ) {
            std::this_thread::sleep_for(
                std::chrono::seconds(1)
            );
        }
    }

    std::signal(SIGINT, previous_sigint_handler);
    std::signal(SIGTERM, previous_sigterm_handler);

    std::cout
        << "WebSocket stream stopped cleanly.\n";
}