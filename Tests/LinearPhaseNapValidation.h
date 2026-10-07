#pragma once
#include <memory>
namespace linearPhaseValidation {
inline int nap(std::ostream& report) {
    using namespace pontedsp::mc2000::dsp;
    int failures=0;
    for(double rate:{44100.,48000.,96000.,192000.}) {
        auto sleeping=std::make_unique<MultiBandCompressor>(), awake=std::make_unique<MultiBandCompressor>();
        GlobalParameters p;p.crossoverMode=CrossoverMode::linearPhase;p.channelMode=ChannelMode::dualMono;p.crossoverHz={20,21,22};
        sleeping->setParameters(p);awake->setParameters(p);awake->setNapEnabled(false);
        sleeping->prepare(rate,64,2);awake->prepare(rate,64,2);
        constexpr int block=2048;std::array<float,block> l{},r{},a{},b{};double error=0;bool slept=false,woke=false;
        const int first=17,second=int(rate*10)+37,ending=second+sleeping->getLatencySamples()+block;
        for(int offset=0;offset<ending;offset+=block) {
            l.fill(0);r.fill(0);a.fill(0);b.fill(0);
            for(int i=0;i<block;++i) if(offset+i==first||offset+i==second)r[std::size_t(i)]=b[std::size_t(i)]=.75f;
            float* napAudio[]{l.data(),r.data()};float* refAudio[]{a.data(),b.data()};
            sleeping->process(napAudio,2,block);awake->process(refAudio,2,block);
            for(int i=0;i<block;++i)error=std::max({error,std::abs(double(l[std::size_t(i)])-a[std::size_t(i)]),std::abs(double(r[std::size_t(i)])-b[std::size_t(i)])});
            if(offset>int(rate)&&offset<second)slept|=sleeping->getActivity()==MultiBandCompressor::Activity::sleeping;
            if(offset<=second&&offset+block>second)woke=sleeping->getActivity()!=MultiBandCompressor::Activity::sleeping;
            if(offset+block>second+sleeping->getLatencySamples()) {
                const int sample=second+sleeping->getLatencySamples()-offset;
                if(sample>=0&&sample<block&&std::abs(r[std::size_t(sample)]-.75f)>1.e-6)++failures;
            }
        }
        if(!slept||!woke||error>1.e-7){++failures;std::cerr<<"FIR nap/wake mismatch rate="<<rate<<" error="<<error<<" slept="<<slept<<" woke="<<woke<<'\n';}
        report<<rate<<",2048,linear_dual,0,"<<slept<<','<<error<<','<<woke<<'\n';
    }
    return failures;
}
}
