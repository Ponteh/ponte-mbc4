#pragma once

#include "FocusTiming.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace pontedsp::gui {

class ControlFocus final : private juce::Timer,
                           private juce::MouseListener,
                           private juce::FocusChangeListener
{
public:
    explicit ControlFocus(juce::Component& owner) : root(owner)
    {
        root.addMouseListener(this, true);
        juce::Desktop::getInstance().addFocusChangeListener(this);
        startTimer(10);
    }
    ~ControlFocus() override
    {
        stopTimer();
        juce::Desktop::getInstance().removeFocusChangeListener(this);
        root.removeMouseListener(this);
    }

    void add(juce::Component& control, std::function<void(bool)> changed = {},
             juce::Component* alias = nullptr, std::function<bool()> held = {})
    {
        control.getProperties().set("pontedspControlActive", false);
        entries.push_back({ &control, alias, std::move(changed), std::move(held) });
    }

    // Shared by native pointer events and the message-thread integration tests.
    void pointerActivity(juce::Component* component, bool immediate = false)
    {
        apply(resolve(component), immediate);
    }

private:
    struct Entry
    {
        juce::Component::SafePointer<juce::Component> control, alias;
        std::function<void(bool)> changed;
        std::function<bool()> held;
    };

    int resolve(juce::Component* component) const
    {
        for (auto* c = component; c != nullptr && c != &root; c = c->getParentComponent())
            for (size_t i = 0; i < entries.size(); ++i)
                if ((entries[i].control == c || entries[i].alias == c)
                    && entries[i].control != nullptr && entries[i].control->isShowing()
                    && entries[i].control->isEnabled())
                    return static_cast<int>(i + 1);
        return 0;
    }

    void apply(int target, bool immediate)
    {
        if (changing) return;
        const auto next = timing.update(target, juce::Time::getMillisecondCounterHiRes(), immediate);
        if (next == displayed) return;
        const juce::ScopedValueSetter<bool> guard(changing, true);
        const auto previous = displayed;
        displayed = next;
        for (const auto id : { previous, next })
        {
            if (id == 0) continue;
            auto& entry = entries[static_cast<size_t>(id - 1)];
            if (entry.control == nullptr) continue;
            const auto active = id == next;
            entry.control->getProperties().set("pontedspControlActive", active);
            if (entry.changed) entry.changed(active);
            entry.control->repaint();
        }
    }

    void timerCallback() override
    {
        auto target = resolve(juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse());
        if (displayed != 0)
        {
            auto& entry = entries[static_cast<size_t>(displayed - 1)];
            if (entry.control != nullptr && entry.control->isShowing() && entry.control->isEnabled())
            {
                if (entry.control->isMouseButtonDown(true)
                    || (target == 0 && entry.held && entry.held()))
                    target = displayed;
            }
        }
        apply(target, false);
    }

    void mouseEnter(const juce::MouseEvent& e) override { pointerActivity(e.eventComponent); }
    void mouseMove(const juce::MouseEvent& e) override { pointerActivity(e.eventComponent); }
    void mouseDown(const juce::MouseEvent& e) override { pointerActivity(e.eventComponent, true); }
    void mouseDrag(const juce::MouseEvent& e) override { pointerActivity(e.eventComponent, true); }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails&) override
    { pointerActivity(e.eventComponent, true); }
    void globalFocusChanged(juce::Component* c) override
    {
        if (const auto target = resolve(c); target != 0) apply(target, true);
    }

    juce::Component& root;
    std::vector<Entry> entries;
    FocusTiming timing;
    int displayed {};
    bool changing {};
};

} // namespace pontedsp::gui
