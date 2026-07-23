#include "http_client.hpp"
#include "order_book.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {

std::vector<PriceLevel> parse_levels(
    const json& snapshot,
    const std::string& side_name
)
{
    if (!snapshot.contains(side_name)) {
        throw std::runtime_error(
            "Order-book response is missing '" +
            side_name +
            "'."
        );
    }

    const json& levels = snapshot.at(side_name);

    if (!levels.is_array()) {
        throw std::runtime_error(
            "'" + side_name + "' must be a JSON array."
        );
    }

    std::vector<PriceLevel> parsed_levels;
    parsed_levels.reserve(levels.size());

    for (const json& level : levels) {
        if (!level.is_object()) {
            throw std::runtime_error(
                "Each order-book level must be an object."
            );
        }

        if (
            !level.contains("price") ||
            !level.contains("size")
        ) {
            throw std::runtime_error(
                "Order-book level is missing price or size."
            );
        }

        const std::string price =
            level.at("price").get<std::string>();

        const std::string size =
            level.at("size").get<std::string>();

        parsed_levels.push_back(
            PriceLevel{
                OrderBook::price_to_ticks(price),
                OrderBook::price_to_ticks(size)
            }
        );
    }

    return parsed_levels;
}

void print_optional_price(
    const std::string& label,
    const std::optional<std::int64_t>& price
)
{
    std::cout << label;

    if (price.has_value()) {
        std::cout << OrderBook::format_price(
            price.value(),
            3
        );
    }
    else {
        std::cout << "N/A";
    }

    std::cout << '\n';
}

void print_order_book(const OrderBook& book)
{
    const std::optional<PriceLevel> best_bid =
        book.best_bid();

    const std::optional<PriceLevel> best_ask =
        book.best_ask();

    std::cout << '\n';

    print_optional_price(
        "Best bid: ",
        best_bid.has_value()
            ? std::optional<std::int64_t>{
                  best_bid->price_ticks
              }
            : std::nullopt
    );

    print_optional_price(
        "Best ask: ",
        best_ask.has_value()
            ? std::optional<std::int64_t>{
                  best_ask->price_ticks
              }
            : std::nullopt
    );

    print_optional_price(
        "Spread:   ",
        book.spread_ticks()
    );

    print_optional_price(
        "Mid:      ",
        book.mid_price_ticks()
    );

    std::cout << '\n';
    std::cout << "Bid levels: "
              << book.bids().size()
              << '\n';

    std::cout << "Ask levels: "
              << book.asks().size()
              << '\n';

    std::cout << "Bid depth:  "
              << OrderBook::format_price(
                     book.bid_depth(),
                     6
                 )
              << '\n';

    std::cout << "Ask depth:  "
              << OrderBook::format_price(
                     book.ask_depth(),
                     6
                 )
              << '\n';
}

void run_book_command(const std::string& token_id)
{
    if (token_id.empty()) {
        throw std::invalid_argument(
            "Token ID cannot be empty."
        );
    }

    const HttpClient client;

    const std::string host =
        "clob.polymarket.com";

    const std::string target =
        "/book?token_id=" + token_id;

    std::cout
        << "Downloading order-book snapshot...\n";

    const std::string response =
        client.get(host, target);

    const json snapshot =
        json::parse(response);

    if (!snapshot.is_object()) {
        throw std::runtime_error(
            "Expected an order-book JSON object."
        );
    }

    std::vector<PriceLevel> bids =
        parse_levels(snapshot, "bids");

    std::vector<PriceLevel> asks =
        parse_levels(snapshot, "asks");

    OrderBook book;

    book.replace_snapshot(
        std::move(bids),
        std::move(asks)
    );

    print_order_book(book);
}

void print_usage(const std::string& program_name)
{
    std::cerr
        << "Usage:\n"
        << "  "
        << program_name
        << " book <token-id>\n";
}

} // namespace

int main(int argc, char* argv[])
{
    try {
        if (argc != 3) {
            print_usage(argv[0]);
            return 1;
        }

        const std::string command = argv[1];
        const std::string token_id = argv[2];

        if (command == "book") {
            run_book_command(token_id);
            return 0;
        }

        std::cerr
            << "Unknown command: "
            << command
            << '\n';

        print_usage(argv[0]);
        return 1;
    }
    catch (const json::exception& error) {
        std::cerr
            << "JSON error: "
            << error.what()
            << '\n';

        return 1;
    }
    catch (const std::exception& error) {
        std::cerr
            << "Error: "
            << error.what()
            << '\n';

        return 1;
    }
}