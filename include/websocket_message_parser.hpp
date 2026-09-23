#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

enum class WebSocketMessageKind {
    pong,
    market_payload,
    invalid
};

struct WebSocketParseResult {
    WebSocketMessageKind kind;
    nlohmann::json payload;
    std::string error_message;
};

class WebSocketMessageParser {
public:
    static WebSocketParseResult parse(
        const std::string& message
    )
    {
        constexpr char whitespace[] =
            " \t\r\n";

        const std::size_t first =
            message.find_first_not_of(whitespace);

        if (first == std::string::npos) {
            return invalid_result(
                "WebSocket message is empty."
            );
        }

        const std::size_t last =
            message.find_last_not_of(whitespace);

        const std::string trimmed =
            message.substr(
                first,
                last - first + 1
            );

        if (
            trimmed.find('\0') !=
            std::string::npos
        ) {
            return invalid_result(
                "WebSocket message contains "
                "an embedded NUL byte."
            );
        }

        if (trimmed == "PONG") {
            return {
                WebSocketMessageKind::pong,
                nullptr,
                {}
            };
        }

        nlohmann::json payload =
            nlohmann::json::parse(
                trimmed,
                nullptr,
                false
            );

        if (payload.is_discarded()) {
            return invalid_result(
                "WebSocket message contains invalid JSON."
            );
        }

        if (
            !payload.is_object() &&
            !payload.is_array()
        ) {
            return invalid_result(
                "WebSocket JSON payload must be "
                "an object or array."
            );
        }

        return {
            WebSocketMessageKind::market_payload,
            std::move(payload),
            {}
        };
    }

private:
    static WebSocketParseResult invalid_result(
        std::string message
    )
    {
        return {
            WebSocketMessageKind::invalid,
            nullptr,
            std::move(message)
        };
    }
};
