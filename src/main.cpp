#include "http_client.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

using json = nlohmann::json;

int main()
{
    try {
        const HttpClient client;

        const std::string host = "gamma-api.polymarket.com";
        const std::string target =
            "/events?active=true&closed=false&limit=10";

        std::cout << "\nFetching live Polymarket events...\n\n";

        const std::string response = client.get(host, target);
        const json events = json::parse(response);

        if (!events.is_array()) {
            throw std::runtime_error("Expected a JSON array.");
        }

        std::cout << "=============================================\n";
        std::cout << "        ACTIVE POLYMARKET EVENTS\n";
        std::cout << "=============================================\n\n";

        int index = 1;

        for (const auto& event : events) {

            const std::string title =
                event.value("title", "Unknown");

            const std::string slug =
                event.value("slug", "N/A");

            const bool active =
                event.value("active", false);

            std::cout << index++ << ". " << title << '\n';
            std::cout << "   Slug   : " << slug << '\n';
            std::cout << "   Status : "
                      << (active ? "Active" : "Inactive")
                      << "\n\n";
        }

        std::cout << "Displayed "
                  << events.size()
                  << " live events.\n";

        return 0;
    }
    catch (const std::exception& e) {

        std::cerr << "Error: "
                  << e.what()
                  << '\n';

        return 1;
    }
}