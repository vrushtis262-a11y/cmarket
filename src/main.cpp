#include "http_client.hpp"

#include <exception>
#include <iostream>
#include <string>

int main()
{
    try {
        const HttpClient client;

        const std::string host = "gamma-api.polymarket.com";
        const std::string target =
            "/events?active=true&closed=false&limit=10";

        std::cout << "Fetching live Polymarket data...\n";

        const std::string response = client.get(host, target);

        std::cout << "Polymarket request succeeded.\n";
        std::cout << "Received " << response.size() << " bytes.\n";

        return 0;
    } catch (const std::exception& exception) {
        std::cerr
            << "Polymarket request failed: "
            << exception.what()
            << '\n';

        return 1;
    }
}