#pragma once

#include <chrono>

class HeartbeatState {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    static constexpr auto heartbeat_interval =
        std::chrono::seconds(10);

    static constexpr auto pong_timeout =
        std::chrono::seconds(20);

    explicit HeartbeatState(
        TimePoint now = Clock::now()
    ) noexcept
        : next_ping_at_(now + heartbeat_interval)
    {
    }

    [[nodiscard]] bool should_send_ping(
        TimePoint now
    ) const noexcept
    {
        return now >= next_ping_at_;
    }

    void on_ping_sent(TimePoint now) noexcept
    {
        if (!awaiting_pong_) {
            awaiting_pong_ = true;
            ping_sent_at_ = now;
        }

        next_ping_at_ =
            now + heartbeat_interval;
    }

    void on_pong_received() noexcept
    {
        awaiting_pong_ = false;
    }

    [[nodiscard]] bool pong_timed_out(
        TimePoint now
    ) const noexcept
    {
        return (
            awaiting_pong_ &&
            now - ping_sent_at_ >= pong_timeout
        );
    }

    [[nodiscard]] bool awaiting_pong() const noexcept
    {
        return awaiting_pong_;
    }

    void reset(TimePoint now) noexcept
    {
        awaiting_pong_ = false;
        ping_sent_at_ = now;

        next_ping_at_ =
            now + heartbeat_interval;
    }

private:
    TimePoint next_ping_at_;
    TimePoint ping_sent_at_{};
    bool awaiting_pong_ = false;
};
