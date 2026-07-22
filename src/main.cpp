#include "http_client.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

int main()
{
    try {
        const HttpClient client;

        const std::string host = "gamma-api.polymarket.com";
        const std::string target =
            "/events?active=true&closed=false&limit=10";

        std::cout << "Fetching live Polymarket data...\n";

        const std::string response = client.get(host, target);
        const json events = json::parse(response);

        if (!events.is_array()) {
            throw std::runtime_error(
                "Unexpected response: expected a JSON array."
            );
        }

        std::cout << "JSON parsed successfully.\n";
        std::cout << "Received " << events.size()
                  << " active Polymarket events.\n";

        return 0;
    } catch (const json::parse_error& exception) {
        std::cerr
            << "Failed to parse Polymarket JSON: "
            << exception.what()
            << '\n';

        return 1;
    } catch (const std::exception& exception) {
        std::cerr
            << "CMarket error: "
            << exception.what()
            << '\n';

        return 1;
    }
}