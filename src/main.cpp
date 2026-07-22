#include "http_client.hpp"

#include <exception>
#include <iostream>

int main()
{
    try {
        const HttpClient client;

        const std::string response = client.get(
            "example.com",
            "/"
        );

        std::cout << "HTTPS request succeeded.\n";
        std::cout << "Received " << response.size() << " bytes.\n";

        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "HTTP request failed: "
                  << exception.what()
                  << '\n';

        return 1;
    }
}