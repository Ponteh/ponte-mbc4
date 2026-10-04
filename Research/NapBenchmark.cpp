#include <juce_dsp/juce_dsp.h>
#include "DSP/MultiBandCompressor.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
using namespace pontedsp::mc2000::dsp;
void benchmarkSilence()
{
    std::vector<float> a(512), b(512);
    float* audio[]{a.data(),b.data()};
    // Same engine, nap on/off; preallocated buffers and no I/O in timed loops.
    for(bool enabled : {false,true})
    {
        MultiBandCompressor bench; bench.setNapEnabled(enabled); bench.prepare(48000,512,2);
        std::fill(a.begin(),a.end(),0.0f); std::fill(b.begin(),b.end(),0.0f);
        bench.process(audio,2,512);
        std::vector<double> times;
        for(int repeat=0;repeat<7;++repeat)
        {
            const auto start=std::chrono::steady_clock::now();
            for(int block=0;block<2000;++block) bench.process(audio,2,512);
            times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        }
        std::sort(times.begin(),times.end());
        std::cout << "nap=" << enabled << " median_ms=" << times[3] << '\n';
    }
}


int main() { juce::ScopedNoDenormals noDenormals; benchmarkSilence(); }
