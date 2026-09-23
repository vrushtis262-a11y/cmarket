#include "websocket_payload_dispatcher.hpp"

#include <cassert>
#include <stdexcept>
#include <vector>

int main()
{
    {
        const nlohmann::json payload =
            nlohmann::json::array(
                {
                    {
                        {"id", 1},
                        {"valid", true}
                    },
                    {
                        {"id", 2},
                        {"valid", false}
                    },
                    {
                        {"id", 3},
                        {"valid", true}
                    }
                }
            );

        std::vector<int> processed_ids;
        int error_count = 0;

        const WebSocketDispatchResult result =
            WebSocketPayloadDispatcher::dispatch(
                payload,
                [&](const nlohmann::json& message) {
                    if (
                        !message.at("valid")
                             .get<bool>()
                    ) {
                        throw std::runtime_error(
                            "Invalid event."
                        );
                    }

                    processed_ids.push_back(
                        message.at("id")
                            .get<int>()
                    );
                },
                [&](const std::exception&) {
                    ++error_count;
                }
            );

        assert(result.processed == 2);
        assert(result.rejected == 1);

        assert(error_count == 1);

        assert(processed_ids.size() == 2);
        assert(processed_ids.at(0) == 1);
        assert(processed_ids.at(1) == 3);
    }

    {
        const nlohmann::json payload = {
            {"id", 7}
        };

        int processed_id = 0;

        const WebSocketDispatchResult result =
            WebSocketPayloadDispatcher::dispatch(
                payload,
                [&](const nlohmann::json& message) {
                    processed_id =
                        message.at("id")
                            .get<int>();
                },
                [](const std::exception&) {
                }
            );

        assert(result.processed == 1);
        assert(result.rejected == 0);
        assert(processed_id == 7);
    }

    {
        const nlohmann::json payload = {
            {"valid", false}
        };

        int error_count = 0;

        const WebSocketDispatchResult result =
            WebSocketPayloadDispatcher::dispatch(
                payload,
                [](const nlohmann::json&) {
                    throw std::runtime_error(
                        "Invalid event."
                    );
                },
                [&](const std::exception&) {
                    ++error_count;
                }
            );

        assert(result.processed == 0);
        assert(result.rejected == 1);
        assert(error_count == 1);
    }

    {
        bool threw = false;

        try {
            WebSocketPayloadDispatcher::dispatch(
                nlohmann::json(42),
                [](const nlohmann::json&) {
                },
                [](const std::exception&) {
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
