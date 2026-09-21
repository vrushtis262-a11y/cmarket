#pragma once

#include <algorithm>

class ReconnectBackoff {
public:
    static constexpr int initial_delay_seconds = 1;
    static constexpr int maximum_delay_seconds = 30;

    [[nodiscard]] int current_delay_seconds() const noexcept
    {
        return current_delay_seconds_;
    }

    void advance() noexcept
    {
        current_delay_seconds_ =
            std::min(
                current_delay_seconds_ * 2,
                maximum_delay_seconds
            );
    }

    void reset() noexcept
    {
        current_delay_seconds_ =
            initial_delay_seconds;
    }

private:
    int current_delay_seconds_ =
        initial_delay_seconds;
};
