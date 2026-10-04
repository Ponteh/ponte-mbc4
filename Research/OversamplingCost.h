#pragma once
#include <chrono>
#include <vector>
// Research-only full DSP prototype with JUCE's two resampler families.
inline int benchmarkOversamplingCost()
{
    using namespace pontedsp::mc2000::dsp;
    using OS=juce::dsp::Oversampling<float>;
    juce::ScopedNoDenormals noDenormals;
    std::cout << "mode,factor,filter,latency_samples,median_us_per_block\n";
    for(int mode=0;mode<3;++mode)
    for(int exponent=0;exponent<=2;++exponent)
    for(int type=0;type<(exponent?2:1);++type)
    {
        const int factor=1<<exponent;
        OS os(2,static_cast<size_t>(exponent),static_cast<OS::FilterType>(type),true,true);
        os.initProcessing(512);
        MultiBandCompressor engine; GlobalParameters p;
        for(auto& band:p.bands) { band.ratio=10;band.thresholdDb=-36;band.attackMs=.25;band.releaseMs=25;band.tcMode=static_cast<TCMode>(mode); }
        engine.setParameters(p);engine.prepare(48000*factor,512*factor,2);
        juce::AudioBuffer<float> buffer(2,512);
        std::array<float,512> source;
        for(size_t i=0;i<source.size();++i) source[i]=float(.7*std::sin(2*juce::MathConstants<double>::pi*7001*i/48000));
        std::vector<double> times;
        for(int repeat=-1;repeat<5;++repeat)
        {
            const auto start=std::chrono::steady_clock::now();
            for(int n=0;n<200;++n)
            {
                buffer.copyFrom(0,0,source.data(),512);buffer.copyFrom(1,0,source.data(),512);
                juce::dsp::AudioBlock<float> block(buffer);
                if(exponent)
                {
                    auto up=os.processSamplesUp(block);
                    float* channels[]{up.getChannelPointer(0),up.getChannelPointer(1)};
                    engine.process(channels,2,static_cast<int>(up.getNumSamples()));os.processSamplesDown(block);
                }
                else { float* channels[]{buffer.getWritePointer(0),buffer.getWritePointer(1)};engine.process(channels,2,512); }
            }
            const auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/200;
            if(repeat>=0) times.push_back(elapsed);
        }
        std::sort(times.begin(),times.end());
        std::cout << mode << ',' << factor << ',' << (exponent?(type?"IIR":"FIR"):"none") << ',' << (exponent?os.getLatencyInSamples():0) << ',' << times[2] << '\n';
    }
    return 0;
}
