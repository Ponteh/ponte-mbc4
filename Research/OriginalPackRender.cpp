#include "ResearchNoDenormals.h"
#include "DSP/MultiBandCompressor.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

// Offline research harness. Input/output: interleaved float32 stereo, job settings
// explicitly supplied by analyse.py. No inference of original plugin settings.
int main(int argc, char** argv)
{
    ResearchNoDenormals noDenormals;
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
        // Optional final field: explicit pack automation, never inferred from a filename.
        std::string rest, automation;
        std::getline(jobs, rest);
        std::istringstream extra(rest);
        extra >> automation;
        if (!automation.empty() && automation != "MANUAL_TIMES" && automation != "MODES"
            && automation != "THRESHOLD" && automation != "CROSSOVER") return 8;
        const auto initial = p;
        std::ifstream input(source, std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0 || input.tellg() % 8 != 0) return 3;
        std::vector<float> audio(static_cast<size_t>(input.tellg()) / sizeof(float));
        input.seekg(0); input.read(reinterpret_cast<char*>(audio.data()), audio.size()*sizeof(float));
        if (!input) return 4;
        MultiBandCompressor engine;
        engine.setParameters(p); engine.prepare(rate, 512, 2);
        float left[512], right[512];
        const auto eventSamples = static_cast<size_t>(std::llround(10.0 * rate));
        int lastEvent = 0;
        for (size_t pos = 0; pos < audio.size()/2;)
        {
            const auto event = static_cast<int>(pos / eventSamples);
            if (!automation.empty() && event != lastEvent && event < 6)
            {
                p = initial;
                constexpr double attacks[] {2.5, .25, 25, 2.5, .25, 25};
                constexpr double releases[] {250, 25, 2500, 250, 2500, 25};
                constexpr double thresholds[] {-27.5, -18, -36, -27.5, -18, -27.5};
                constexpr TCMode modes[] {TCMode::automatic, TCMode::type1, TCMode::type2,
                    TCMode::automatic, TCMode::type1, TCMode::automatic};
                for (auto& b : p.bands)
                {
                    if (automation == "MANUAL_TIMES") { b.attackMs=attacks[event]; b.releaseMs=releases[event]; }
                    if (automation == "MODES") b.tcMode=modes[event];
                    if (automation == "THRESHOLD") b.thresholdDb=thresholds[event];
                }
                if (automation == "CROSSOVER") p.crossoverHz[0] = event == 1 || event == 3 ? 200.0 : 100.0;
                engine.setParameters(p);
                lastEvent = event;
            }
            auto count = std::min(size_t(512), audio.size()/2-pos);
            if (!automation.empty()) count = std::min(count, eventSamples - pos % eventSamples);
            const auto n = static_cast<int>(count);
            for (int i=0; i<n; ++i) { left[i]=audio[2*(pos+i)]; right[i]=audio[2*(pos+i)+1]; }
            float* channels[] { left, right };
            engine.process(channels, 2, n);
            for (int i=0; i<n; ++i)
            {
                if (!std::isfinite(left[i]) || !std::isfinite(right[i])) return 5;
                audio[2*(pos+i)]=left[i]; audio[2*(pos+i)+1]=right[i];
            }
            pos += count;
        }
        std::ofstream output(destination, std::ios::binary);
        output.write(reinterpret_cast<const char*>(audio.data()), audio.size()*sizeof(float));
        if (!output) return 6;
        std::cout << destination << std::endl;
    }
    return jobs.eof() ? 0 : 7;
}
