#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif
#include "DSP/MultiBandCompressor.h"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <numbers>
#include <vector>
using namespace pontedsp::mc2000::dsp;
static double threadCpuSeconds() {
#if defined(_WIN32)
    FILETIME created,ended,kernel,user;GetThreadTimes(GetCurrentThread(),&created,&ended,&kernel,&user);
    const auto ticks=[](FILETIME t){return (std::uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
    return double(ticks(kernel)+ticks(user))*1.e-7;
#else
    return double(std::clock())/CLOCKS_PER_SEC;
#endif
}
int main(int argc,char** argv) {
    std::ofstream report(argc>1?argv[1]:"release025-benchmark.csv");
    report<<"rate,profile,channel_mode,block,latency_samples,wall_fraction,thread_cpu_fraction,p99_ms,max_ms,deadline_ms\n";
    for(int rate:{44100,48000,96000,192000})for(int profile:{0,1}) {
        MultiBandCompressor engine;GlobalParameters p;p.crossoverMode=CrossoverMode::linearPhase;
        p.crossoverHz=profile==0?std::array<double,3>{100,1000,10000}:std::array<double,3>{20,21,22};
        for(auto& b:p.bands){b.thresholdDb=-30;b.ratio=4;b.bite=10;}
        engine.setParameters(p);engine.prepare(rate,2048,2);engine.setNapEnabled(false);
        for(int channels:{0,1})for(int block:{64,512,2048}) {
            p.channelMode=static_cast<ChannelMode>(channels);engine.setParameters(p);engine.reset();
            std::vector<float> left(std::size_t(block),.4f),right(std::size_t(block),.03f),keyL(std::size_t(block),.6f),keyR(std::size_t(block),.008f);
            float* program[]{left.data(),right.data()};const float* key[]{keyL.data(),keyR.data()};
            std::vector<double> times;double total=0;const double cpuStart=threadCpuSeconds();
            for(int offset=0;offset<rate;offset+=block) {
                for(int i=0;i<block;++i){const double t=double(offset+i)/rate;left[std::size_t(i)]=float(.4*std::sin(2*std::numbers::pi*315*t));right[std::size_t(i)]=float(.03*std::sin(2*std::numbers::pi*1700*t));keyL[std::size_t(i)]=left[std::size_t(i)];keyR[std::size_t(i)]=right[std::size_t(i)];}
                const auto tick=std::chrono::steady_clock::now();engine.process(program,2,key,2,std::min(block,rate-offset));
                const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-tick).count();times.push_back(ms);total+=ms;
            }
            std::sort(times.begin(),times.end());
            report<<rate<<','<<profile<<','<<channels<<','<<block<<','<<engine.getLatencySamples()<<','<<total/1000<<','<<threadCpuSeconds()-cpuStart<<','<<times[std::min(times.size()-1,std::size_t(times.size()*.99))]<<','<<times.back()<<','<<1000.*block/rate<<'\n';
        }
        report.flush();std::cout<<"Benchmark complete rate="<<rate<<" profile="<<profile<<'\n';
    }
    return report.good()?0:1;
}
