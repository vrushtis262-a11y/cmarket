#include "websocket_market_state.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

int main()
{
    {
        WebSocketMarketState state(
            {
                "token-1",
                "token-2",
                "token-3"
            }
        );

        assert(state.size() == 3);

        assert(state.contains("token-1"));
        assert(state.contains("token-2"));
        assert(state.contains("token-3"));

        assert(!state.contains("token-4"));
    }

    {
        WebSocketMarketState state(
            {
                "token-1",
                "token-2",
                "token-1"
            }
        );

        assert(state.size() == 2);
    }

    {
        WebSocketMarketState state(
            {
                "token-1",
                "token-2"
            }
        );

        assert(
            !state.at("token-1")
                 .snapshot_received
        );

        assert(
            !state.at("token-2")
                 .snapshot_received
        );

        state.at("token-1")
            .snapshot_received = true;

        assert(
            state.at("token-1")
                .snapshot_received
        );

        assert(
            !state.at("token-2")
                 .snapshot_received
        );
    }

    {
        WebSocketMarketState state(
            {
                "token-1",
                "token-2"
            }
        );

        state.at("token-1")
            .book
            .update_bid(
                OrderBook::price_to_ticks("0.50"),
                OrderBook::quantity_to_fixed("10")
            );

        assert(
            state.at("token-1")
                .book
                .best_bid()
                .has_value()
        );

        assert(
            !state.at("token-2")
                 .book
                 .best_bid()
                 .has_value()
        );
    }

    {
        WebSocketMarketState state(
            {"token-1"}
        );

        bool threw = false;

        try {
            state.at("unknown-token");
        }
        catch (const std::invalid_argument&) {
            threw = true;
        }

        assert(threw);
    }

    {
        bool threw = false;

        try {
            WebSocketMarketState state(
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
            WebSocketMarketState state(
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
