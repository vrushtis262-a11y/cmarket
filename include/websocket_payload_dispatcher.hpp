#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <stdexcept>

struct WebSocketDispatchResult {
    std::size_t processed = 0;
    std::size_t rejected = 0;
};

class WebSocketPayloadDispatcher {
public:
    template <
        typename MessageHandler,
        typename ErrorHandler
    >
    static WebSocketDispatchResult dispatch(
        const nlohmann::json& payload,
        MessageHandler&& message_handler,
        ErrorHandler&& error_handler
    )
    {
        WebSocketDispatchResult result;

        if (payload.is_array()) {
            for (
                const nlohmann::json& message :
                payload
            ) {
                dispatch_one(
                    message,
                    message_handler,
                    error_handler,
                    result
                );
            }

            return result;
        }

        if (payload.is_object()) {
            dispatch_one(
                payload,
                message_handler,
                error_handler,
                result
            );

            return result;
        }

        throw std::invalid_argument(
            "WebSocket payload must be "
            "an object or array."
        );
    }

private:
    template <
        typename MessageHandler,
        typename ErrorHandler
    >
    static void dispatch_one(
        const nlohmann::json& message,
        MessageHandler& message_handler,
        ErrorHandler& error_handler,
        WebSocketDispatchResult& result
    )
    {
        try {
            message_handler(message);
            ++result.processed;
        }
        catch (const std::exception& error) {
            ++result.rejected;
            error_handler(error);
        }
    }
};
