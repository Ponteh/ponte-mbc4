#include <juce_dsp/juce_dsp.h>
#include "DSP/MultiBandCompressor.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include "OversamplingCost.h"

int main() { return benchmarkOversamplingCost(); }
