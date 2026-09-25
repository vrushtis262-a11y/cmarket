#pragma once

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

class WebSocketSubscription {
public:
    static std::string build_market(
        const std::vector<std::string>& token_ids
    )
    {
        if (token_ids.empty()) {
            throw std::invalid_argument(
                "At least one token ID is required."
            );
        }

        std::vector<std::string> unique_token_ids;
        unique_token_ids.reserve(token_ids.size());

        std::unordered_set<std::string> seen;

        for (const std::string& token_id : token_ids) {
            if (token_id.empty()) {
                throw std::invalid_argument(
                    "Token ID must not be empty."
                );
            }

            if (seen.insert(token_id).second) {
                unique_token_ids.push_back(token_id);
            }
        }

        const nlohmann::json message = {
            {"assets_ids", unique_token_ids},
            {"type", "market"}
        };

        return message.dump();
    }
};
