#pragma once

#include <atomic>

namespace pontedsp::mc2000::dsp {

// One audio producer, one GUI consumer. A GUI exchange cannot erase a peak
// published after it. With one producer, strong CAS needs at most one retry:
// the only competing write is a reset to the floor.
class MeterPeak final
{
public:
    explicit MeterPeak(float minimum = -100.0f) noexcept : floor(minimum), peak(minimum) {}
    void publish(float value) noexcept
    {
        auto previous = peak.load(std::memory_order_relaxed);
        while (value > previous
               && !peak.compare_exchange_strong(previous, value, std::memory_order_relaxed)) {}
    }
    float consume() noexcept { return peak.exchange(floor, std::memory_order_relaxed); }
    void reset() noexcept { peak.store(floor, std::memory_order_relaxed); }

private:
    static_assert(std::atomic<float>::is_always_lock_free);
    const float floor;
    std::atomic<float> peak;
};

} // namespace pontedsp::mc2000::dsp
