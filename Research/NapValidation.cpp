#include "ResearchNoDenormals.h"
#include "DSP/MultiBandCompressor.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
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
        std::fill(a.begin(),a.end(),0); std::fill(b.begin(),b.end(),0);
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

int main(int argc, char** argv)
{
    ResearchNoDenormals noDenormals;
    if (argc > 1 && std::string(argv[1]) == "--benchmark") { benchmarkSilence(); return 0; }
    std::ofstream report(argc > 1 ? argv[1] : "nap-validation.csv");
    report << "rate,block,mode,external,sleep_seconds,max_audio_error,woke\n";
    int failures = 0;
    const auto check = [&](bool ok, const char* message) { if (!ok) { ++failures; std::cerr << message << '\n'; } };
    for (auto rate : {44100,48000,96000,192000})
    for (auto block : {64,512,2048})
    for (auto mode : {TCMode::type1,TCMode::type2,TCMode::automatic})
    {
        MultiBandCompressor nap, reference;
        reference.setNapEnabled(false);
        GlobalParameters p;
        p.crossoverHz = {20,785,10000};
        for (auto& b : p.bands) { b.thresholdDb=-36; b.ratio=4; b.bite=10; b.attackMs=.25; b.releaseMs=250; b.tcMode=mode; }
        nap.setParameters(p); reference.setParameters(p);
        nap.prepare(rate,block,2); reference.prepare(rate,block,2);
        std::vector<float> a(block), b(block), c(block), d(block), key(block);
        const bool external = block == 512;
        double error=0, slept=-1;
        bool woke=false;
        // Two pulses separated by 11 seconds: long enough for all BITE memories.
        for (int offset=0; offset<rate*13; offset+=block)
        {
            const int n=std::min(block,rate*13-offset);
            for (int i=0;i<n;++i)
            {
                const auto t=double(offset+i)/rate;
                const float v = t < .2 || t >= 12 ? float(.5*std::sin(2*3.141592653589793*315*t)) : 0.f;
                a[i]=c[i]=v; b[i]=d[i]=-.37f*v; key[i]=v;
            }
            float* left[]{a.data(),b.data()}; float* right[]{c.data(),d.data()};
            const float* detector[]{key.data(),key.data()};
            nap.setParameters(p); reference.setParameters(p); // unchanged snapshots must not wake
            nap.process(left,2,external?detector:nullptr,external?2:0,n);
            reference.process(right,2,external?detector:nullptr,external?2:0,n);
            for(int i=0;i<n;++i) error=std::max({error,std::abs(double(a[i])-c[i]),std::abs(double(b[i])-d[i])});
            if (nap.getActivity()==MultiBandCompressor::Activity::sleeping && slept<0) slept=double(offset+n)/rate;
            if (offset>=rate*12 && nap.getActivity()!=MultiBandCompressor::Activity::sleeping) woke=true;
        }
        check(slept>0 && slept<12,"did not enter nap after drained silence");
        check(woke,"did not wake on first audio block");
        check(error<1.e-9,"nap audio mismatch");
        report << rate << ',' << block << ',' << int(mode) << ',' << external << ',' << slept << ',' << error << ',' << woke << '\n';
    }
    // Empty engine sleeps, all raw inputs wake it, including key and tiny signals.
    MultiBandCompressor engine;
    engine.prepare(48000,512,2);
    std::vector<float> a(512),b(512),key(512);
    float* audio[]{a.data(),b.data()}; const float* detector[]{key.data(),key.data()};
    engine.process(audio,2,512);
    check(engine.getActivity()==MultiBandCompressor::Activity::sleeping,"initial silence did not sleep");
    a[0]=1.e-30f; engine.process(audio,2,512);
    check(a[0]!=0,"tiny first sample lost on wake");
    engine.reset(); std::fill(a.begin(),a.end(),0); std::fill(b.begin(),b.end(),0);
    engine.process(audio,2,detector,2,512); key[0]=.5f;
    engine.process(audio,2,detector,2,512);
    check(engine.getActivity()==MultiBandCompressor::Activity::active,"live sidechain failed to keep awake");
    engine.reset(); key[0]=0; engine.process(audio,2,512);
    auto p=engine.getParameters(); p.bands[0].enabled=false; p.bands[3].solo=true; p.crossoverHz[0]=200;
    engine.setParameters(p);
    check(engine.getActivity()==MultiBandCompressor::Activity::active,"parameters failed to wake");
    engine.process(audio,2,512);
    check(engine.getActivity()==MultiBandCompressor::Activity::draining,"routing ramp slept too early");
    // Every possible wake position in a 512-sample block, including right-only input.
    for(int position=0;position<512;++position)
    {
        MultiBandCompressor n,r; r.setNapEnabled(false);
        n.prepare(48000,512,2); r.prepare(48000,512,2);
        std::vector<float> x(512),y(512),u(512),v(512);
        float* na[]{x.data(),y.data()}; float* ra[]{u.data(),v.data()};
        n.process(na,2,512); r.process(ra,2,512);
        y[position]=v[position]=.75f;
        n.process(na,2,512); r.process(ra,2,512);
        check(x==u && y==v,"wake impulse position not bit exact");
    }
    // Long release must drain naturally, never expire on a fixed nap timeout.
    {
        MultiBandCompressor n,r; r.setNapEnabled(false);
        GlobalParameters params;
        for(auto& b:params.bands) { b.ratio=10; b.thresholdDb=-48; b.releaseMs=2500; b.attackMs=.25; }
        n.setParameters(params); r.setParameters(params); n.prepare(48000,512,2); r.prepare(48000,512,2);
        std::vector<float> x(512),y(512),u(512),v(512);
        float* na[]{x.data(),y.data()}; float* ra[]{u.data(),v.data()};
        double error=0; bool sleep=false;
        for(int offset=0;offset<48000*100;offset+=512)
        {
            for(int i=0;i<512;++i) x[i]=y[i]=u[i]=v[i]=offset<48000 ? .8f : 0.f;
            n.process(na,2,512); r.process(ra,2,512);
            for(int i=0;i<512;++i) error=std::max(error,std::abs(double(x[i])-u[i]));
            if(offset==48000/512*512+512) check(n.getActivity()!=MultiBandCompressor::Activity::sleeping,"long release cut short");
            sleep |= n.getActivity()==MultiBandCompressor::Activity::sleeping;
        }
        check(sleep && error<1.e-9,"long release nap mismatch");
        params.numBands=2;params.inputGainDb=24;params.outputGainDb=24;
        params.bands[0].enabled=false;params.bands[1].solo=true;params.bands[1].tcMode=TCMode::automatic;
        n.setParameters(params);r.setParameters(params);
        for(int k=0;k<100;++k)
        {
            for(int i=0;i<512;++i) x[i]=y[i]=u[i]=v[i]=float(.1*std::sin((k*512+i)*.1));
            n.process(na,2,512);r.process(ra,2,512);
            for(int i=0;i<512;++i) check(std::abs(double(x[i])-u[i])<1.e-7,"sleep parameter/mode/band change regression");
        }
    }
    benchmarkSilence();
    std::cout << "failures=" << failures << '\n';
    return failures ? 1 : 0;
}
