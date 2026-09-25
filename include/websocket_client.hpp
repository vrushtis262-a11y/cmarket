#pragma once

#include <string>
#include <vector>

class WebSocketClient {
public:
    void stream_market(
        const std::string& token_id
    ) const;

    void stream_market(
        const std::vector<std::string>& token_ids
    ) const;

private:
    static std::string build_subscription_message(
        const std::vector<std::string>& token_ids
    );
};
