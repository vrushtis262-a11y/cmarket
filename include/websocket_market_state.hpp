#pragma once

#include "order_book.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

struct WebSocketTokenState {
    OrderBook book;
    bool snapshot_received = false;
};

class WebSocketMarketState {
public:
    explicit WebSocketMarketState(
        const std::vector<std::string>& token_ids
    )
    {
        if (token_ids.empty()) {
            throw std::invalid_argument(
                "At least one token ID is required."
            );
        }

        for (const std::string& token_id : token_ids) {
            if (token_id.empty()) {
                throw std::invalid_argument(
                    "Token ID must not be empty."
                );
            }

            states_.try_emplace(token_id);
        }
    }

    [[nodiscard]] bool contains(
        const std::string& token_id
    ) const noexcept
    {
        return states_.contains(token_id);
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return states_.size();
    }

    WebSocketTokenState& at(
        const std::string& token_id
    )
    {
        const auto iterator =
            states_.find(token_id);

        if (iterator == states_.end()) {
            throw std::invalid_argument(
                "Received event for unsubscribed token ID: " +
                token_id
            );
        }

        return iterator->second;
    }

    const WebSocketTokenState& at(
        const std::string& token_id
    ) const
    {
        const auto iterator =
            states_.find(token_id);

        if (iterator == states_.end()) {
            throw std::invalid_argument(
                "Received event for unsubscribed token ID: " +
                token_id
            );
        }

        return iterator->second;
    }

private:
    std::unordered_map<
        std::string,
        WebSocketTokenState
    > states_;
};
