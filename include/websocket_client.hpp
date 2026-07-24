#pragma once

#include <string>

class WebSocketClient {
public:
    void stream_market(
        const std::string& token_id
    ) const;

private:
    static std::string build_subscription_message(
        const std::string& token_id
    );
};