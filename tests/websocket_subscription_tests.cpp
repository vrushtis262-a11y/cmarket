#include "websocket_subscription.hpp"

#include <nlohmann/json.hpp>

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

int main()
{
    {
        const std::string message =
            WebSocketSubscription::build_market(
                {"token-1"}
            );

        const nlohmann::json payload =
            nlohmann::json::parse(message);

        assert(payload.at("type") == "market");

        assert(
            payload.at("assets_ids").size() == 1
        );

        assert(
            payload.at("assets_ids").at(0) ==
            "token-1"
        );
    }

    {
        const std::string message =
            WebSocketSubscription::build_market(
                {
                    "token-1",
                    "token-2",
                    "token-3"
                }
            );

        const nlohmann::json payload =
            nlohmann::json::parse(message);

        assert(
            payload.at("assets_ids").size() == 3
        );

        assert(
            payload.at("assets_ids").at(0) ==
            "token-1"
        );

        assert(
            payload.at("assets_ids").at(1) ==
            "token-2"
        );

        assert(
            payload.at("assets_ids").at(2) ==
            "token-3"
        );
    }

    {
        const std::string message =
            WebSocketSubscription::build_market(
                {
                    "token-1",
                    "token-2",
                    "token-1"
                }
            );

        const nlohmann::json payload =
            nlohmann::json::parse(message);

        assert(
            payload.at("assets_ids").size() == 2
        );

        assert(
            payload.at("assets_ids").at(0) ==
            "token-1"
        );

        assert(
            payload.at("assets_ids").at(1) ==
            "token-2"
        );
    }

    {
        bool threw = false;

        try {
            WebSocketSubscription::build_market(
                {}
            );
        }
        catch (const std::invalid_argument&) {
            threw = true;
        }

        assert(threw);
    }

    {
        bool threw = false;

        try {
            WebSocketSubscription::build_market(
                {
                    "token-1",
                    ""
                }
            );
        }
        catch (const std::invalid_argument&) {
            threw = true;
        }

        assert(threw);
    }

    return 0;
}
