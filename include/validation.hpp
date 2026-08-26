#ifndef CMARKET_VALIDATION_HPP
#define CMARKET_VALIDATION_HPP

#include <cstdint>
#include <stdexcept>
#include <string>

namespace validation {

inline void require_positive(
    std::int64_t value,
    const std::string& field_name
)
{
    if (value <= 0) {
        throw std::invalid_argument(
            field_name + " must be positive."
        );
    }
}

inline void validate_market_order(
    std::int64_t quantity
)
{
    require_positive(
        quantity,
        "Market order quantity"
    );
}

inline void validate_limit_order(
    std::int64_t price_ticks,
    std::int64_t quantity
)
{
    require_positive(
        price_ticks,
        "Limit price"
    );

    require_positive(
        quantity,
        "Limit quantity"
    );
}

inline void validate_modify_order(
    std::int64_t price_ticks,
    std::int64_t quantity
)
{
    require_positive(
        price_ticks,
        "Modified limit price"
    );

    require_positive(
        quantity,
        "Modified limit quantity"
    );
}

inline void validate_trade(
    std::int64_t price_ticks,
    std::int64_t quantity
)
{
    require_positive(
        price_ticks,
        "Trade price"
    );

    require_positive(
        quantity,
        "Trade quantity"
    );
}

} // namespace validation

#endif