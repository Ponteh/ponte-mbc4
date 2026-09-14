#pragma once

namespace pontedsp::gui {

// One visual focus per editor. Keyboard focus is an event, not a permanent latch.
class FocusTiming
{
public:
    int update(int target, double now, bool immediate = false) noexcept
    {
        if (target == 0)
        {
            candidate = 0;
            if (now - lastActivity >= 100.0) active = 0;
        }
        else
        {
            if (candidate != target)
            {
                candidate = target;
                entered = now;
            }
            if (immediate || active != 0 || now - entered >= 30.0)
            {
                active = target;
                lastActivity = now;
            }
        }
        return active;
    }
    int current() const noexcept { return active; }

private:
    int active {}, candidate {};
    double entered {}, lastActivity {};
};

} // namespace pontedsp::gui
