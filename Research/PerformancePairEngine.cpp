#include "DSP/MultiBandCompressor.h"
#define JOIN_IMPL(a,b) a##b
#define JOIN(a,b) JOIN_IMPL(a,b)
using namespace pontedsp::mc2000::dsp;
struct State { MultiBandCompressor engine; GlobalParameters p; };
extern "C" void* JOIN(BENCH_PREFIX,_create)(int rate,int block,int mode,int id)
{
    auto* s=new State; s->p.numBands=2+id%3;
    for(auto& b:s->p.bands) { b.ratio=4;b.thresholdDb=-30;b.tcMode=static_cast<TCMode>(mode);b.bite=5;b.attackMs=2.5;b.releaseMs=500; }
    s->engine.setParameters(s->p);s->engine.prepare(rate,block,2);return s;
}
extern "C" void JOIN(BENCH_PREFIX,_destroy)(void* state) { delete static_cast<State*>(state); }
extern "C" void JOIN(BENCH_PREFIX,_process)(void* state,float** audio,const float* const* key,int n,int step,bool automation)
{
    auto& s=*static_cast<State*>(state);auto& p=s.p;
    if(automation)
    {
        p.bands[1].attackMs=step%2?.25:25;p.bands[1].releaseMs=step%2?25:2500;p.bands[1].bite=step%2?10:1;
        p.bands[0].gainDb=step%2?3:-3;p.crossoverHz[0]=step%2?180:80;
        p.bands[0].enabled=step!=4;p.bands[1].solo=step==6;
        if(step==7)p.bands[1].tcMode=TCMode::automatic;
    }
    s.engine.setParameters(p);s.engine.process(audio,2,key,key?2:0,n);
}
