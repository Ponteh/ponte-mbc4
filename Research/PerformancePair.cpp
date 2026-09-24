#include "ResearchNoDenormals.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif
static unsigned long long threadCycles()
{
#if defined(_WIN32)
    ULONG64 cycles=0;
    QueryThreadCycleTime(GetCurrentThread(), &cycles);
    return cycles;
#else
    return 0;
#endif
}
extern "C" void* baseline_create(int,int,int,int);
extern "C" void* current_create(int,int,int,int);
extern "C" void baseline_destroy(void*);
extern "C" void current_destroy(void*);
extern "C" void baseline_process(void*,float**,const float* const*,int,int,bool);
extern "C" void current_process(void*,float**,const float* const*,int,int,bool);
int main(int argc,char** argv)
{
    ResearchNoDenormals noDenormals;
    if(argc!=2)return 1;
    std::ofstream csv(argv[1]);csv<<"case,rate,block,mode,automation,repeat,before_us,after_us,before_p99,after_p99,bit_equal,max_error,before_cycles,after_cycles\n";
    int id=0;bool allEqual=true;
    for(int rate:{44100,48000,96000})for(int block:{64,512,1024})for(int mode:{0,1,2})for(bool automation:{false,true})
    {
        ++id;
        std::vector<float> source(rate),key(rate);
        for(int i=0;i<rate;++i) { const auto t=double(i)/rate;source[i]=float((t<.3||t>.55?.4:.005)*(.7*std::sin(2*3.141592653589793*315*t)+.3*std::sin(2*3.141592653589793*2100*t)));key[i]=float(.3*std::sin(2*3.141592653589793*90*t)); }
        for(int repeat=-1;repeat<5;++repeat)
        {
            void* states[]{baseline_create(rate,block,mode,id),current_create(rate,block,mode,id)};
            std::array<std::vector<float>,4> audio;
            for(auto& a:audio)a.resize(block);
            std::array<std::vector<double>,2> timings;
            std::array<unsigned long long,2> cycles{};
            for(auto& ts:timings)ts.reserve(rate/block+1);
            bool equal=true;double error=0;
            for(int offset=0;offset<rate;offset+=block)
            {
                const int n=std::min(block,rate-offset);
                for(int i=0;i<n;++i) { audio[0][i]=audio[2][i]=source[offset+i];audio[1][i]=audio[3][i]=-.5f*source[offset+i]; }
                const float* detector[]{key.data()+offset,key.data()+offset};
                for(int order=0;order<2;++order)
                {
                    const int which=(order+offset/block+repeat+1)%2;
                    float* channels[]{audio[2*which].data(),audio[2*which+1].data()};
                    const auto firstCycle=threadCycles();
                    const auto start=std::chrono::steady_clock::now();
                    (which?current_process:baseline_process)(states[which],channels,id%2?detector:nullptr,n,offset/(rate/8),automation);
                    timings[which].push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
                    cycles[which]+=threadCycles()-firstCycle;
                }
                equal &= std::memcmp(audio[0].data(),audio[2].data(),n*sizeof(float))==0 && std::memcmp(audio[1].data(),audio[3].data(),n*sizeof(float))==0;
                for(int i=0;i<n;++i)error=std::max({error,std::abs(double(audio[0][i])-audio[2][i]),std::abs(double(audio[1][i])-audio[3][i])});
            }
            baseline_destroy(states[0]);current_destroy(states[1]);allEqual &= equal;
            if(repeat<0)continue;
            std::array<double,2> totals{},p99{};
            for(int i=0;i<2;++i) { for(auto t:timings[i])totals[i]+=t;std::sort(timings[i].begin(),timings[i].end());p99[i]=timings[i][size_t(.99*(timings[i].size()-1))]; }
            csv<<id<<','<<rate<<','<<block<<','<<mode<<','<<automation<<','<<repeat<<','<<totals[0]<<','<<totals[1]<<','<<p99[0]<<','<<p99[1]<<','<<equal<<','<<error<<','<<cycles[0]<<','<<cycles[1]<<'\n';
        }
    }
    std::cout<<"54 paired cases; bit_equal="<<allEqual<<'\n';return allEqual?0:2;
}
