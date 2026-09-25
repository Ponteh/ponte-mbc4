#include "ResearchNoDenormals.h"
#include "DSP/MultiBandCompressor.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// Research-only 1 ms meter snapshots, without GUI smoothing. Float32 stereo input.
int main(int argc, char** argv)
{
    if (argc != 4) return 1;
    ResearchNoDenormals noDenormals;
    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
    if (!file || file.tellg() <= 0 || file.tellg() % 8 != 0) return 2;
    std::vector<float> audio(static_cast<size_t>(file.tellg()) / sizeof(float));
    file.seekg(0); file.read(reinterpret_cast<char*>(audio.data()), audio.size() * sizeof(float));
    if (!file) return 3;
    using namespace pontedsp::mc2000::dsp;
    GlobalParameters p;
    p.numBands = 4; p.crossoverHz = {100,785,10000};
    for (auto& b : p.bands)
    {
        b.enabled=true; b.solo=false; b.gainDb=0; b.thresholdDb=-27.5;
        b.ratio=2; b.knee=0; b.bite=1; b.attackMs=2.5;
        b.releaseMs=std::stod(argv[3]); b.tcMode=TCMode::type1;
    }
    MultiBandCompressor engine;
    engine.setParameters(p); engine.prepare(48000,48,2);
    std::ofstream out(argv[2]); out << std::setprecision(10) << "seconds,in2,out2,gr2,in3,out3,gr3\n";
    for (size_t pos=0; pos<audio.size()/2; pos+=48)
    {
        const auto n=static_cast<int>(std::min(size_t(48),audio.size()/2-pos));
        float l[48],r[48];
        for (int i=0;i<n;++i) { l[i]=audio[2*(pos+i)];r[i]=audio[2*(pos+i)+1]; }
        float* ch[]{l,r};engine.process(ch,2,n);
        out << (pos+n)/48000.0;
        for (int band : {1,2})
        {
            const auto m=engine.getBandMeter(band);
            out << ',' << m.inputDb << ',' << m.outputDb << ',' << m.gainReductionDb;
        }
        out << '\n';
    }
    return out ? 0 : 4;
}
