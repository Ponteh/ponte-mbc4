// Research-only Auto hypotheses; no candidate changes the production algorithm.
#include "DSP/CrossoverNetwork.h"
#include "DSP/Db.h"
#include <array>
extern "C" __declspec(dllexport) void evaluateNoiseBite(const float* input, float* output,
    int frames, double sampleRate, double attackUs, double releaseMs, int soloBand, double biteMs)
{
    using namespace pontedsp::mc2000::dsp;
    CrossoverNetwork crossover; crossover.prepare(sampleRate,2); crossover.setFrequencies({100,785,10000});
    std::array<double,4> control{1,1,1,1};
    const auto attack=attackUs>0 ? std::exp(-1/(sampleRate*attackUs*1.e-6)) : 0;
    const auto release=std::exp(-1/(sampleRate*releaseMs*.001));
    std::array<double,4> smoothGr{};
    const auto biteAttack=biteMs>0 ? std::exp(-1/(sampleRate*biteMs*.001)) : 0.;
    const auto threshold=decibelsToGain(-27.5);
    for(int i=0;i<frames;++i)
    {
        std::array<double,4> l,r;
        crossover.processSample(0,input[2*i],l); crossover.processSample(1,input[2*i+1],r);
        double left=0,right=0;
        for(int b=0;b<4;++b)
        {
            const auto target=std::max(1.,std::max(std::abs(l[b]),std::abs(r[b]))/threshold);
            auto& q=control[b]; q=target>q ? attack*q+(1-attack)*target : 1+release*(q-1);
            const auto normal=10*std::log10(q);
            auto& gr=smoothGr[b];
            gr=normal>gr ? biteAttack*gr+(1-biteAttack)*normal : normal;
            const auto gain=biteMs>0 ? decibelsToGain(-gr) : 1/std::sqrt(q);
            if(soloBand<0 || soloBand==b) { left+=l[b]*gain; right+=r[b]*gain; }
        }
        output[2*i]=float(left); output[2*i+1]=float(right);
    }
}

extern "C" __declspec(dllexport) void evaluateNoise(const float* input, float* output,
    int frames, double sampleRate, double attackUs, double releaseMs, int soloBand)
{ evaluateNoiseBite(input,output,frames,sampleRate,attackUs,releaseMs,soloBand,0); }
