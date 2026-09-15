#ifndef CMARKET_ORDER_MANAGER_HPP
#define CMARKET_ORDER_MANAGER_HPP

#include "limit_order.hpp"

#include <cstddef>
#include <set>
#include <unordered_map>
#include <vector>

class OrderManager {
private:
    using OrderIndexMap =
        std::unordered_map<
            OrderId,
            std::size_t
        >;

    struct PriorityKey {
        std::int64_t price_ticks;
        SequenceNumber sequence_number;
        OrderId order_id;
    };

    struct BuyPriorityCompare {
        [[nodiscard]]
        bool operator()(
            const PriorityKey& lhs,
            const PriorityKey& rhs
        ) const noexcept
        {
            if (
                lhs.price_ticks !=
                rhs.price_ticks
            ) {
                return lhs.price_ticks >
                    rhs.price_ticks;
            }

            if (
                lhs.sequence_number !=
                rhs.sequence_number
            ) {
                return lhs.sequence_number <
                    rhs.sequence_number;
            }

            return lhs.order_id <
                rhs.order_id;
        }
    };

    struct SellPriorityCompare {
        [[nodiscard]]
        bool operator()(
            const PriorityKey& lhs,
            const PriorityKey& rhs
        ) const noexcept
        {
            if (
                lhs.price_ticks !=
                rhs.price_ticks
            ) {
                return lhs.price_ticks <
                    rhs.price_ticks;
            }

            if (
                lhs.sequence_number !=
                rhs.sequence_number
            ) {
                return lhs.sequence_number <
                    rhs.sequence_number;
            }

            return lhs.order_id <
                rhs.order_id;
        }
    };

    using BuyPriorityIndex =
        std::set<
            PriorityKey,
            BuyPriorityCompare
        >;

    using SellPriorityIndex =
        std::set<
            PriorityKey,
            SellPriorityCompare
        >;

public:
    struct Removal {
        LimitOrder removed_order;
        std::size_t original_index = 0;
        bool swapped = false;
        OrderId swapped_order_id = 0;

        OrderIndexMap::node_type
            removed_index_node;

        BuyPriorityIndex::node_type
            removed_buy_priority_node;

        SellPriorityIndex::node_type
            removed_sell_priority_node;
    };

    void add_order(
        const LimitOrder& order
    );

    [[nodiscard]]
    bool cancel_order(
        OrderId order_id
    );

    [[nodiscard]]
    bool cancel_order(
        OrderId order_id,
        Removal& removal
    );

    void restore_removal(
        Removal& removal
    );

    [[nodiscard]]
    bool update_sequence_number(
        OrderId order_id,
        SequenceNumber new_sequence_number
    );

    [[nodiscard]]
    LimitOrder* find_order(
        OrderId order_id
    ) noexcept;

    [[nodiscard]]
    const LimitOrder* find_order(
        OrderId order_id
    ) const noexcept;

    [[nodiscard]]
    const LimitOrder*
    best_buy_order() const noexcept;

    [[nodiscard]]
    const LimitOrder*
    best_sell_order() const noexcept;

    [[nodiscard]]
    const std::vector<LimitOrder>&
    orders() const noexcept;

private:
    static void validate_order(
        const LimitOrder& order
    );

    [[nodiscard]]
    static PriorityKey make_priority_key(
        const LimitOrder& order
    ) noexcept;

    std::vector<LimitOrder> orders_;

    OrderIndexMap order_indices_;

    BuyPriorityIndex buy_priority_index_;

    SellPriorityIndex sell_priority_index_;
};

#endif