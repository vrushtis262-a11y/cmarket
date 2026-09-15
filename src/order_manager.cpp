#include "order_manager.hpp"
#include "validation.hpp"

#include <stdexcept>
#include <utility>

void OrderManager::validate_order(
    const LimitOrder& order
)
{
    if (order.order_id == 0) {
        throw std::invalid_argument(
            "Order ID must be non-zero."
        );
    }

    validation::require_positive(
        order.price_ticks,
        "Order price"
    );

    validation::require_positive(
        order.original_quantity,
        "Order original quantity"
    );

    validation::require_positive(
        order.remaining_quantity,
        "Active order remaining quantity"
    );

    if (
        order.remaining_quantity >
        order.original_quantity
    ) {
        throw std::invalid_argument(
            "Order remaining quantity cannot exceed "
            "original quantity."
        );
    }

    if (order.sequence_number == 0) {
        throw std::invalid_argument(
            "Order sequence number must be non-zero."
        );
    }
}

OrderManager::PriorityKey
OrderManager::make_priority_key(
    const LimitOrder& order
) noexcept
{
    return PriorityKey{
        .price_ticks = order.price_ticks,
        .sequence_number =
            order.sequence_number,
        .order_id = order.order_id
    };
}

void OrderManager::add_order(
    const LimitOrder& order
)
{
    validate_order(order);

    if (
        order_indices_.find(
            order.order_id
        ) != order_indices_.end()
    ) {
        throw std::invalid_argument(
            "Order ID already exists."
        );
    }

    orders_.push_back(order);

    bool id_index_added = false;
    bool priority_index_added = false;

    try {
        order_indices_.emplace(
            order.order_id,
            orders_.size() - 1
        );

        id_index_added = true;

        const PriorityKey key =
            make_priority_key(order);

        if (order.side == OrderSide::Buy) {
            const auto result =
                buy_priority_index_.insert(
                    key
                );

            if (!result.second) {
                throw std::logic_error(
                    "Buy priority key already exists."
                );
            }
        }
        else {
            const auto result =
                sell_priority_index_.insert(
                    key
                );

            if (!result.second) {
                throw std::logic_error(
                    "Sell priority key already exists."
                );
            }
        }

        priority_index_added = true;
    }
    catch (...) {
        if (priority_index_added) {
            const PriorityKey key =
                make_priority_key(order);

            if (order.side == OrderSide::Buy) {
                buy_priority_index_.erase(
                    key
                );
            }
            else {
                sell_priority_index_.erase(
                    key
                );
            }
        }

        if (id_index_added) {
            order_indices_.erase(
                order.order_id
            );
        }

        orders_.pop_back();

        throw;
    }
}

bool OrderManager::cancel_order(
    OrderId order_id
)
{
    const auto index_iterator =
        order_indices_.find(
            order_id
        );

    if (
        index_iterator ==
        order_indices_.end()
    ) {
        return false;
    }

    const std::size_t index =
        index_iterator->second;

    const LimitOrder order_to_remove =
        orders_[index];

    const PriorityKey priority_key =
        make_priority_key(
            order_to_remove
        );

    if (order_to_remove.side == OrderSide::Buy) {
        const std::size_t erased =
            buy_priority_index_.erase(
                priority_key
            );

        if (erased != 1U) {
            throw std::logic_error(
                "Buy order is missing from priority index."
            );
        }
    }
    else {
        const std::size_t erased =
            sell_priority_index_.erase(
                priority_key
            );

        if (erased != 1U) {
            throw std::logic_error(
                "Sell order is missing from priority index."
            );
        }
    }

    const std::size_t last_index =
        orders_.size() - 1;

    if (index != last_index) {
        orders_[index] =
            std::move(
                orders_[last_index]
            );

        const auto moved_iterator =
            order_indices_.find(
                orders_[index].order_id
            );

        if (
            moved_iterator ==
            order_indices_.end()
        ) {
            throw std::logic_error(
                "Moved order is missing from index."
            );
        }

        moved_iterator->second =
            index;
    }

    orders_.pop_back();

    order_indices_.erase(
        index_iterator
    );

    return true;
}

bool OrderManager::cancel_order(
    OrderId order_id,
    Removal& removal
)
{
    const auto index_iterator =
        order_indices_.find(
            order_id
        );

    if (
        index_iterator ==
        order_indices_.end()
    ) {
        return false;
    }

    const std::size_t index =
        index_iterator->second;

    const std::size_t last_index =
        orders_.size() - 1;

    removal.removed_order =
        orders_[index];

    removal.original_index =
        index;

    removal.swapped =
        index != last_index;

    removal.swapped_order_id =
        removal.swapped
            ? orders_[last_index].order_id
            : 0;

    const PriorityKey priority_key =
        make_priority_key(
            removal.removed_order
        );

    if (
        removal.removed_order.side ==
        OrderSide::Buy
    ) {
        removal.removed_buy_priority_node =
            buy_priority_index_.extract(
                priority_key
            );

        if (
            removal.removed_buy_priority_node.empty()
        ) {
            throw std::logic_error(
                "Removed buy priority node is missing."
            );
        }
    }
    else {
        removal.removed_sell_priority_node =
            sell_priority_index_.extract(
                priority_key
            );

        if (
            removal.removed_sell_priority_node.empty()
        ) {
            throw std::logic_error(
                "Removed sell priority node is missing."
            );
        }
    }

    if (removal.swapped) {
        orders_[index] =
            std::move(
                orders_[last_index]
            );

        const auto moved_iterator =
            order_indices_.find(
                orders_[index].order_id
            );

        if (
            moved_iterator ==
            order_indices_.end()
        ) {
            throw std::logic_error(
                "Moved order is missing from index."
            );
        }

        moved_iterator->second =
            index;
    }

    orders_.pop_back();

    removal.removed_index_node =
        order_indices_.extract(
            index_iterator
        );

    if (
        removal.removed_index_node.empty()
    ) {
        throw std::logic_error(
            "Removed order index node is missing."
        );
    }

    return true;
}

void OrderManager::restore_removal(
    Removal& removal
)
{
    if (
        removal.removed_index_node.empty()
    ) {
        throw std::logic_error(
            "Removal has already been restored."
        );
    }

    if (
        order_indices_.find(
            removal.removed_order.order_id
        ) != order_indices_.end()
    ) {
        throw std::logic_error(
            "Removed order already exists."
        );
    }

    const bool is_buy =
        removal.removed_order.side ==
        OrderSide::Buy;

    if (
        is_buy &&
        removal.removed_buy_priority_node.empty()
    ) {
        throw std::logic_error(
            "Removed buy priority node is missing."
        );
    }

    if (
        !is_buy &&
        removal.removed_sell_priority_node.empty()
    ) {
        throw std::logic_error(
            "Removed sell priority node is missing."
        );
    }

    const std::size_t restored_last_index =
        orders_.size();

    orders_.push_back(
        removal.removed_order
    );

    if (removal.swapped) {
        if (
            removal.original_index >=
            restored_last_index
        ) {
            orders_.pop_back();

            throw std::logic_error(
                "Invalid removal restoration index."
            );
        }

        if (
            orders_[
                removal.original_index
            ].order_id !=
            removal.swapped_order_id
        ) {
            orders_.pop_back();

            throw std::logic_error(
                "Swapped order does not match "
                "removal record."
            );
        }

        std::swap(
            orders_[removal.original_index],
            orders_[restored_last_index]
        );
    }

    removal.removed_index_node.mapped() =
        removal.original_index;

    const auto index_insert_result =
        order_indices_.insert(
            std::move(
                removal.removed_index_node
            )
        );

    if (!index_insert_result.inserted) {
        throw std::logic_error(
            "Removed order index could not "
            "be restored."
        );
    }

    if (is_buy) {
        const auto priority_insert_result =
            buy_priority_index_.insert(
                std::move(
                    removal.removed_buy_priority_node
                )
            );

        if (!priority_insert_result.inserted) {
            throw std::logic_error(
                "Removed buy priority could not "
                "be restored."
            );
        }
    }
    else {
        const auto priority_insert_result =
            sell_priority_index_.insert(
                std::move(
                    removal.removed_sell_priority_node
                )
            );

        if (!priority_insert_result.inserted) {
            throw std::logic_error(
                "Removed sell priority could not "
                "be restored."
            );
        }
    }

    if (!removal.swapped) {
        return;
    }

    const auto swapped_iterator =
        order_indices_.find(
            removal.swapped_order_id
        );

    if (
        swapped_iterator ==
        order_indices_.end()
    ) {
        throw std::logic_error(
            "Swapped order is missing from index."
        );
    }

    swapped_iterator->second =
        restored_last_index;
}

bool OrderManager::update_sequence_number(
    OrderId order_id,
    SequenceNumber new_sequence_number
)
{
    if (new_sequence_number == 0) {
        throw std::invalid_argument(
            "Order sequence number must be non-zero."
        );
    }

    LimitOrder* order =
        find_order(
            order_id
        );

    if (order == nullptr) {
        return false;
    }

    if (
        order->sequence_number ==
        new_sequence_number
    ) {
        return true;
    }

    const PriorityKey old_key =
        make_priority_key(
            *order
        );

    PriorityKey new_key =
        old_key;

    new_key.sequence_number =
        new_sequence_number;

    if (order->side == OrderSide::Buy) {
        const auto insert_result =
            buy_priority_index_.insert(
                new_key
            );

        if (!insert_result.second) {
            throw std::logic_error(
                "Updated buy priority key already exists."
            );
        }

        const std::size_t erased =
            buy_priority_index_.erase(
                old_key
            );

        if (erased != 1U) {
            buy_priority_index_.erase(
                new_key
            );

            throw std::logic_error(
                "Existing buy priority key is missing."
            );
        }
    }
    else {
        const auto insert_result =
            sell_priority_index_.insert(
                new_key
            );

        if (!insert_result.second) {
            throw std::logic_error(
                "Updated sell priority key already exists."
            );
        }

        const std::size_t erased =
            sell_priority_index_.erase(
                old_key
            );

        if (erased != 1U) {
            sell_priority_index_.erase(
                new_key
            );

            throw std::logic_error(
                "Existing sell priority key is missing."
            );
        }
    }

    order->sequence_number =
        new_sequence_number;

    return true;
}

LimitOrder* OrderManager::find_order(
    OrderId order_id
) noexcept
{
    const auto iterator =
        order_indices_.find(
            order_id
        );

    if (
        iterator ==
        order_indices_.end()
    ) {
        return nullptr;
    }

    return &orders_[iterator->second];
}

const LimitOrder* OrderManager::find_order(
    OrderId order_id
) const noexcept
{
    const auto iterator =
        order_indices_.find(
            order_id
        );

    if (
        iterator ==
        order_indices_.end()
    ) {
        return nullptr;
    }

    return &orders_[iterator->second];
}

const LimitOrder*
OrderManager::best_buy_order() const noexcept
{
    if (buy_priority_index_.empty()) {
        return nullptr;
    }

    return find_order(
        buy_priority_index_.begin()->order_id
    );
}

const LimitOrder*
OrderManager::best_sell_order() const noexcept
{
    if (sell_priority_index_.empty()) {
        return nullptr;
    }

    return find_order(
        sell_priority_index_.begin()->order_id
    );
}

const std::vector<LimitOrder>&
OrderManager::orders() const noexcept
{
    return orders_;
}