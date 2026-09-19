#include "DSP/MultiBandCompressor.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

// Offline research harness. Input/output: interleaved float32 stereo, job settings
// explicitly supplied by analyse.py. No inference of original plugin settings.
int main(int argc, char** argv)
{
    using namespace pontedsp::mc2000::dsp;
    if (argc != 2) return 1;
    std::ifstream jobs(argv[1]);
    std::string source, destination;
    while (jobs >> std::quoted(source) >> std::quoted(destination))
    {
        double rate;
        GlobalParameters p;
        jobs >> rate >> p.numBands >> p.inputGainDb >> p.outputGainDb;
        for (auto& hz : p.crossoverHz) jobs >> hz;
        for (auto& b : p.bands)
        {
            int mode;
            jobs >> b.enabled >> b.solo >> b.gainDb >> b.thresholdDb >> b.ratio
                 >> b.knee >> b.bite >> b.attackMs >> b.releaseMs >> mode;
            b.tcMode = static_cast<TCMode>(mode);
        }
        if (!jobs) return 2;
        std::ifstream input(source, std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0 || input.tellg() % 8 != 0) return 3;
        std::vector<float> audio(static_cast<size_t>(input.tellg()) / sizeof(float));
        input.seekg(0); input.read(reinterpret_cast<char*>(audio.data()), audio.size()*sizeof(float));
        if (!input) return 4;
        MultiBandCompressor engine;
        engine.setParameters(p); engine.prepare(rate, 512, 2);
        float left[512], right[512];
        for (size_t pos = 0; pos < audio.size()/2; pos += 512)
        {
            const auto n = static_cast<int>(std::min(size_t(512), audio.size()/2-pos));
            for (int i=0; i<n; ++i) { left[i]=audio[2*(pos+i)]; right[i]=audio[2*(pos+i)+1]; }
            float* channels[] { left, right };
            engine.process(channels, 2, n);
            for (int i=0; i<n; ++i)
            {
                if (!std::isfinite(left[i]) || !std::isfinite(right[i])) return 5;
                audio[2*(pos+i)]=left[i]; audio[2*(pos+i)+1]=right[i];
            }
        }
        std::ofstream output(destination, std::ios::binary);
        output.write(reinterpret_cast<const char*>(audio.data()), audio.size()*sizeof(float));
        if (!output) return 6;
        std::cout << destination << std::endl;
    }
    return jobs.eof() ? 0 : 7;
}
