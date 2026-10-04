#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

// Same floating-point environment as the plugin callback: JUCE saves/restores
// MXCSR on Intel and FPCR on ARM64. The archived SSE-only helper was a no-op on ARM.
using TestNoDenormals = juce::ScopedNoDenormals;
