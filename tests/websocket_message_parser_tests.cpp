#include "websocket_message_parser.hpp"

#include <cassert>
#include <string>

int main()
{
    {
        const auto result =
            WebSocketMessageParser::parse(
                "PONG"
            );

        assert(
            result.kind ==
            WebSocketMessageKind::pong
        );
    }

    {
        const auto result =
            WebSocketMessageParser::parse(
                "  \nPONG\r\n"
            );

        assert(
            result.kind ==
            WebSocketMessageKind::pong
        );
    }

    {
        const auto result =
            WebSocketMessageParser::parse(
                R"({"event_type":"book"})"
            );

        assert(
            result.kind ==
            WebSocketMessageKind::market_payload
        );

        assert(
            result.payload.is_object()
        );

        assert(
            result.payload.at("event_type") ==
            "book"
        );
    }

    {
        const auto result =
            WebSocketMessageParser::parse(
                R"([{"event_type":"book"}])"
            );

        assert(
            result.kind ==
            WebSocketMessageKind::market_payload
        );

        assert(
            result.payload.is_array()
        );

        assert(
            result.payload.size() == 1
        );
    }

    {
        const auto result =
            WebSocketMessageParser::parse(
                "  \n{\"event_type\":\"book\"}\t "
            );

        assert(
            result.kind ==
            WebSocketMessageKind::market_payload
        );

        assert(
            result.payload.is_object()
        );
    }

    {
        const auto result =
            WebSocketMessageParser::parse(
                R"({"event_type":)"
            );

        assert(
            result.kind ==
            WebSocketMessageKind::invalid
        );

        assert(
            !result.error_message.empty()
        );
    }

    {
        const auto result =
            WebSocketMessageParser::parse(
                ""
            );

        assert(
            result.kind ==
            WebSocketMessageKind::invalid
        );
    }

    {
        const auto result =
            WebSocketMessageParser::parse(
                "   \n\t\r"
            );

        assert(
            result.kind ==
            WebSocketMessageKind::invalid
        );
    }

    {
        const auto result =
            WebSocketMessageParser::parse(
                R"("hello")"
            );

        assert(
            result.kind ==
            WebSocketMessageKind::invalid
        );
    }

    {
        const auto result =
            WebSocketMessageParser::parse(
                "42"
            );

        assert(
            result.kind ==
            WebSocketMessageKind::invalid
        );
    }

    {
        const auto result =
            WebSocketMessageParser::parse(
                "true"
            );

        assert(
            result.kind ==
            WebSocketMessageKind::invalid
        );
    }

    {
        const auto result =
            WebSocketMessageParser::parse(
                "null"
            );

        assert(
            result.kind ==
            WebSocketMessageKind::invalid
        );
    }

    {
        const auto result =
            WebSocketMessageParser::parse(
                R"({"event_type":"book"} trailing)"
            );

        assert(
            result.kind ==
            WebSocketMessageKind::invalid
        );
    }

    {
        const auto result =
            WebSocketMessageParser::parse(
                R"({"event_type":"book"}{"event_type":"book"})"
            );

        assert(
            result.kind ==
            WebSocketMessageKind::invalid
        );
    }

    {
        const std::string message{
            '{',
            '"',
            'x',
            '"',
            ':',
            '1',
            '}',
            '\0',
            'x'
        };

        const auto result =
            WebSocketMessageParser::parse(
                message
            );

        assert(
            result.kind ==
            WebSocketMessageKind::invalid
        );
    }

    return 0;
}
