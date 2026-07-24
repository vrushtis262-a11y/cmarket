#pragma once

#include <string>

class WebSocketClient {
public:
    void stream_market(
        const std::string& token_id
    ) const;
};