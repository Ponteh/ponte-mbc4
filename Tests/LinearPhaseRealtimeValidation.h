#pragma once
#include <chrono>
#include <thread>
#include <fstream>
namespace linearPhaseValidation {
#if defined(MC2000_TECHNICAL_TESTS)
inline bool realtime() {
    using namespace pontedsp::mc2000;
    bool passed=true;std::uint64_t calls=0;double maxError=0;realtimeAudit::Counts total;
    const auto audit=[&](){const auto c=realtimeAudit::counts;total.allocations+=c.allocations;total.frees+=c.frees;total.locks+=c.locks;total.waits+=c.waits;total.io+=c.io;return c.allocations+c.frees+c.locks+c.waits+c.io==0;};
    juce::MidiBuffer midi;
    for(int rate:{44100,48000,96000,192000}) for(int mainChannels:{1,2}) for(int keyChannels:{0,1,2}) {
        auto instance=std::make_unique<PonteMC2000AudioProcessor>();auto& p=*instance;
        auto layout=p.getBusesLayout();layout.inputBuses.set(0,mainChannels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());layout.outputBuses.set(0,layout.inputBuses[0]);
        layout.inputBuses.set(1,keyChannels==0?juce::AudioChannelSet::disabled():keyChannels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
        passed=p.setBusesLayout(layout)&&passed;
        technicalValidation::parameter(p,parameters::crossoverMode,1);p.prepareToPlay(rate,64);
        parameters::SnapshotReader reader(p.state);parameters::LinkRuntime link;
        auto ref=std::make_unique<dsp::MultiBandCompressor>();ref->setParameters(reader.read(link));ref->prepare(rate,64,mainChannels);
        juce::AudioBuffer<float> audio(mainChannels+keyChannels,2048),expected(mainChannels,2048);
        for(int bands:{2,3,4})for(int channels:{0,1})for(int tc:{0,1,2}) {
            technicalValidation::parameter(p,parameters::bandCount,float(bands-2));technicalValidation::parameter(p,parameters::channelMode,float(channels));
            for(int b=0;b<4;++b){technicalValidation::parameter(p,parameters::bandId(b,"sidechainSource"),keyChannels>0&&b%2==channels?1.f:0.f);technicalValidation::parameter(p,parameters::bandId(b,"ratio"),4);technicalValidation::parameter(p,parameters::bandId(b,"thresholdDb"),-36);technicalValidation::parameter(p,parameters::bandId(b,"tcMode"),float(tc));technicalValidation::parameter(p,parameters::bandId(b,"bite"),tc==0?1.f:10.f);}
            const int length=p.getLatencySamples()+4096;
            for(int offset=0,step=0;offset<length;++step) {
                constexpr std::array<int,5> sizes{1,17,64,512,2048};const int n=std::min(sizes[std::size_t(step%5)],length-offset);
                audio.setSize(mainChannels+keyChannels,n,false,false,true);expected.setSize(mainChannels,n,false,false,true);
                for(int c=0;c<mainChannels+keyChannels;++c)for(int i=0;i<n;++i){float v=float((c%2?.007:.4)*std::sin((offset+i)*.073*(c+1)));if(step==4&&i==0)v=std::numeric_limits<float>::infinity();audio.setSample(c,i,v);if(c<mainChannels)expected.setSample(c,i,v);}
                std::array<float*,2> program {expected.getWritePointer(0), mainChannels==2?expected.getWritePointer(1):nullptr};std::array<const float*,2> key{};for(int c=0;c<keyChannels;++c)key[std::size_t(c)]=audio.getReadPointer(mainChannels+c);
                ref->setParameters(reader.read(link));ref->process(program.data(),mainChannels,key.data(),keyChannels,n);
                {realtimeAudit::Guard guard;p.processBlock(audio,midi);}
                passed=audit()&&passed;
                for(int c=0;c<mainChannels;++c)for(int i=0;i<n;++i){const double e=std::abs(double(audio.getSample(c,i))-expected.getSample(c,i));maxError=std::max(maxError,e);passed=passed&&std::isfinite(audio.getSample(c,i))&&e<1.e-7;}
                ++calls;offset+=n;
            }
        }
        // Native bypass uses preallocated scratch even for oversized blocks.
        audio.clear();{realtimeAudit::Guard guard;static_cast<juce::AudioProcessor*>(&p)->processBlockBypassed(audio,midi);}
        passed=audit()&&passed;++calls;
        if(rate==48000&&mainChannels==2&&keyChannels==2) {
            CrossoverPlot plot(p);plot.setSize(500,200);
            const auto originalVersion=p.getEngine().getLinearResponseVersion();
            technicalValidation::parameter(p,parameters::crossoverId(0),20);
            bool changed=false;
            for(int iteration=0;iteration<300;++iteration) {
                audio.setSize(4,512,false,false,true);
                for(int c=0;c<4;++c)for(int i=0;i<512;++i)audio.setSample(c,i,float(.2*std::sin((iteration*512+i)*.073*(c+1))));
                {realtimeAudit::Guard guard;p.processBlock(audio,midi);}
                passed=audit()&&passed;++calls;
                plot.updateSpectrum(.01);
                changed|=p.getEngine().getLinearResponseVersion()!=originalVersion;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            passed=changed&&passed;
        }
        passed=p.getEngine().getLinearSchedulingOverruns()==0&&passed;
    }
    std::ofstream report("linear-phase-rt.json");
    report<<"{\"callbacks\":"<<calls<<",\"maxAudioError\":"<<maxError<<",\"allocations\":"<<total.allocations<<",\"frees\":"<<total.frees<<",\"locks\":"<<total.locks<<",\"waits\":"<<total.waits<<",\"io\":"<<total.io<<",\"passed\":"<<(passed?"true":"false")<<"}\n";
    passed=report.good()&&passed;
    std::cout<<"Linear Phase RT matrix callbacks="<<calls<<" error="<<maxError<<" passed="<<passed<<'\n';return passed;
}
#endif
}
