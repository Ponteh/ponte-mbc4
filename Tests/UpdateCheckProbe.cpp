#include "UI/ReleaseCheck.h"
#include <iostream>

// Manual smoke test, excluded from CTest/CI: tests never depend on GitHub uptime.
int main()
{
    pontedsp::gui::ReleaseCheck check;
    for (int i = 0; i < 60; ++i)
    {
        const auto version = check.latest();
        if (version.isNotEmpty()) { std::cout << version << '\n'; return 0; }
        juce::Thread::sleep(100);
    }
    std::cerr << "No release received (offline/timeout/rate limit); GUI remains usable.\n";
    return 1;
}
