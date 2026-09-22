#include "heartbeat_state.hpp"

#include <cassert>
#include <chrono>

int main()
{
    using namespace std::chrono_literals;

    const HeartbeatState::TimePoint start{};

    HeartbeatState heartbeat(start);

    assert(
        !heartbeat.should_send_ping(
            start + 9s
        )
    );

    assert(
        heartbeat.should_send_ping(
            start + 10s
        )
    );

    heartbeat.on_ping_sent(
        start + 10s
    );

    assert(
        heartbeat.awaiting_pong()
    );

    assert(
        heartbeat.should_send_ping(
            start + 20s
        )
    );

    heartbeat.on_ping_sent(
        start + 20s
    );

    assert(
        !heartbeat.pong_timed_out(
            start + 29s
        )
    );

    assert(
        heartbeat.pong_timed_out(
            start + 30s
        )
    );

    heartbeat.on_pong_received();

    assert(
        !heartbeat.awaiting_pong()
    );

    assert(
        !heartbeat.pong_timed_out(
            start + 60s
        )
    );

    heartbeat.on_ping_sent(
        start + 70s
    );

    assert(
        heartbeat.awaiting_pong()
    );

    heartbeat.reset(
        start + 75s
    );

    assert(
        !heartbeat.awaiting_pong()
    );

    assert(
        !heartbeat.should_send_ping(
            start + 84s
        )
    );

    assert(
        heartbeat.should_send_ping(
            start + 85s
        )
    );

    return 0;
}
