#include "ResearchNoDenormals.h"
#include "DSP/MultiBandCompressor.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

// Same harness compiled against either a frozen baseline Source directory or
// the current engine. No I/O/allocation inside the timed process() region.
int main(int argc, char** argv)
{
    ResearchNoDenormals noDenormals;
    using namespace pontedsp::mc2000::dsp;
    if (argc != 2) { std::cerr << "Usage: MC2000Performance output-directory\n"; return 1; }
    std::filesystem::create_directories(argv[1]);
    std::ofstream csv(std::filesystem::path(argv[1]) / "timing.csv");
    csv << "case,repeat,mode,rate,block,automation,total_us,p99_us,max_us\n";
    int id = 0;
    for (const int rate : { 44100, 48000, 96000 })
    for (const int block : { 64, 512, 1024 })
    for (const auto mode : { TCMode::type1, TCMode::type2, TCMode::automatic })
    for (const bool automation : { false, true })
    {
        ++id;
        const int frames = rate;
        std::vector<float> source(frames), key(frames), left(block), right(block), output(frames * 2);
        for (int i = 0; i < frames; ++i)
        {
            const auto t = static_cast<double>(i) / rate;
            source[i] = static_cast<float>((t < .3 || t > .55 ? .4 : .005)
                * (.7 * std::sin(2 * 3.141592653589793 * 315 * t)
                  + .3 * std::sin(2 * 3.141592653589793 * 2100 * t)));
            key[i] = static_cast<float>(.3 * std::sin(2 * 3.141592653589793 * 90 * t));
        }
        for (int repeat = -1; repeat < 5; ++repeat) // one warm-up, then five measurements
        {
            GlobalParameters p;
            p.numBands = 2 + id % 3;
            for (auto& band : p.bands)
            {
                band.ratio = 4; band.thresholdDb = -30; band.tcMode = mode;
                band.bite = 5; band.attackMs = 2.5; band.releaseMs = 500;
            }
            MultiBandCompressor engine;
            engine.setParameters(p); engine.prepare(rate, block, 2);
            std::vector<double> times; times.reserve(frames / block + 1);
            for (int offset = 0; offset < frames; offset += block)
            {
                const auto n = std::min(block, frames - offset);
                for (int i = 0; i < n; ++i) { left[i] = source[offset+i]; right[i] = -.5f * left[i]; }
                if (automation)
                {
                    const auto step = offset / (rate / 8);
                    p.bands[1].attackMs = step % 2 ? .25 : 25;
                    p.bands[1].releaseMs = step % 2 ? 25 : 2500;
                    p.bands[1].bite = step % 2 ? 10 : 1;
                    p.bands[0].gainDb = step % 2 ? 3 : -3;
                    p.crossoverHz[0] = step % 2 ? 180 : 80;
                    p.bands[0].enabled = step != 4;
                    p.bands[1].solo = step == 6;
                    if (step == 7) p.bands[1].tcMode = TCMode::automatic;
                }
                engine.setParameters(p);
                float* channels[] { left.data(), right.data() };
                const float* detector[] { key.data() + offset, key.data() + offset };
                const auto start = std::chrono::steady_clock::now();
                engine.process(channels, 2, id % 2 ? detector : nullptr, id % 2 ? 2 : 0, n);
                const auto end = std::chrono::steady_clock::now();
                times.push_back(std::chrono::duration<double, std::micro>(end-start).count());
                for (int i = 0; i < n; ++i)
                {
                    output[2*(offset+i)] = left[i]; output[2*(offset+i)+1] = right[i];
                    if (!std::isfinite(left[i]) || !std::isfinite(right[i])) return 2;
                }
            }
            if (repeat < 0) continue;
            double total = 0; for (const auto t : times) total += t;
            std::sort(times.begin(), times.end());
            csv << id << ',' << repeat << ',' << static_cast<int>(mode) << ',' << rate << ','
                << block << ',' << automation << ',' << total << ','
                << times[static_cast<std::size_t>(.99 * (times.size()-1))] << ',' << times.back() << '\n';
            if (repeat == 0)
            {
                std::ofstream raw(std::filesystem::path(argv[1]) / (std::to_string(id)+".f32"), std::ios::binary);
                raw.write(reinterpret_cast<const char*>(output.data()), static_cast<std::streamsize>(output.size()*sizeof(float)));
            }
        }
    }
    std::cout << id << " cases, five measured repeats each; raw float stereo output saved\n";
}
