#pragma once
#include "RealtimeAudit.h"
#include "Diagnostics.h"
#include <fstream>
#include <chrono>
#include <limits>
#include <thread>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#undef min
#undef max
#endif

namespace technicalValidation {
static_assert(std::atomic<float>::is_always_lock_free && std::atomic<double>::is_always_lock_free
    && std::atomic<int>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);
using namespace pontedsp::mc2000;
inline void parameter(PonteMC2000AudioProcessor& p,const juce::String& id,float v) {
    auto* control=p.state.getParameter(id);control->setValueNotifyingHost(control->convertTo0to1(v));
}
inline double threadCpuMs() {
#if defined(_WIN32)
    FILETIME c,e,k,u;GetThreadTimes(GetCurrentThread(),&c,&e,&k,&u);
    auto value=[](FILETIME t){return (static_cast<std::uint64_t>(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
    return (value(k)+value(u))*.0001;
#else
    return 0;
#endif
}
inline std::uint64_t privateBytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX p{};p.cb=sizeof(p);
    GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&p),sizeof(p));return p.PrivateUsage;
#else
    return 0;
#endif
}
inline std::uint64_t threadCycles() {
#if defined(_WIN32)
    ULONG64 cycles=0;QueryThreadCycleTime(GetCurrentThread(),&cycles);return cycles;
#else
    return 0;
#endif
}
inline int matrix(const char* path) {
    juce::ScopedNoDenormals noDenormals;
    const auto hooks=realtimeAudit::installImportHooks();
    bool ok=realtimeAudit::selfTest();
    if(!ok) { std::cerr<<"RT audit self-test failed\n";return 1; }
    std::uint64_t calls=0,violations=0,cases=0;double maxError=0;
    realtimeAudit::Counts totals{};
    juce::MidiBuffer midi;
    const auto check=[&](bool condition,const char* label) { if(!condition) { ok=false;if(violations++<12)std::cerr<<label<<'\n'; } };
    // Every combination, not just one dimension varied at a time.
    for(int rate:{44100,48000,88200,96000,192000})
    for(int block:{32,64,128,256,512,1024,2048,257})
    for(int mainChannels:{1,2})for(int keyChannels:{0,1,2})
    {
        PonteMC2000AudioProcessor p;
        auto layout=p.getBusesLayout();
        layout.inputBuses.set(0,mainChannels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
        layout.outputBuses.set(0,layout.inputBuses[0]);
        layout.inputBuses.set(1,keyChannels==0?juce::AudioChannelSet::disabled():keyChannels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
        check(p.setBusesLayout(layout),"bus layout rejected");
        for(int bands:{2,3,4})for(int mode:{0,1,2})
        {
            ++cases;
            parameter(p,parameters::bandCount,float(bands-2));
            parameter(p,parameters::linkMaster,0);
            for(int b=0;b<4;++b) {
                parameter(p,parameters::bandId(b,"enabled"),1);
                parameter(p,parameters::bandId(b,"solo"),0);
                parameter(p,parameters::bandId(b,"ratio"),4);
                parameter(p,parameters::bandId(b,"thresholdDb"),-30);
                parameter(p,parameters::bandId(b,"tcMode"),float(mode));
                parameter(p,parameters::bandId(b,"bite"),float(1+((bands+mode)%3)*4.5));
            }
            p.prepareToPlay(rate,block);
            parameters::SnapshotReader reader(p.state);parameters::LinkRuntime link;
            dsp::MultiBandCompressor reference;reference.setNapEnabled(false);
            reference.setParameters(reader.read(link));reference.prepare(rate,block,mainChannels);
            juce::AudioBuffer<float> buffer(mainChannels+keyChannels,block), expected(mainChannels,block);
            if(cases%2)p.addSpectrumConsumer();
            for(int step=0;step<10;++step) {
                const int n=step==0?0:step==2?1:step==4?std::max(1,block-13):block;
                buffer.setSize(mainChannels+keyChannels,n,false,false,true);
                expected.setSize(mainChannels,n,false,false,true);
                buffer.clear();expected.clear();
                if(step>1)for(int ch=0;ch<mainChannels+keyChannels;++ch)for(int i=0;i<n;++i) {
                    float v=float(.3*std::sin((step*block+i)*.039*(ch+1)));
                    if(step==8 && ch<mainChannels)v=0; // live key with silent program
                    if(step==9 && i<2)v=i==0?std::numeric_limits<float>::quiet_NaN():std::numeric_limits<float>::infinity();
                    buffer.setSample(ch,i,v);if(ch<mainChannels)expected.setSample(ch,i,v);
                }
                if(step==5) { parameter(p,parameters::bandId(0,"enabled"),0);parameter(p,parameters::bandId(1,"solo"),1); }
                if(step==6) { parameter(p,parameters::bandId(0,"enabled"),1);parameter(p,parameters::bandId(1,"solo"),0);parameter(p,parameters::bandId(1,"tcMode"),float((mode+1)%3)); }
                reference.setParameters(reader.read(link));
                std::array<const float*,2> key{};
                for(int ch=0;ch<keyChannels;++ch)key[ch]=buffer.getReadPointer(mainChannels+ch);
                std::array<float*,2> expectedPointers{expected.getWritePointer(0),mainChannels==2?expected.getWritePointer(1):nullptr};
                reference.process(expectedPointers.data(),mainChannels,keyChannels?key.data():nullptr,keyChannels,n);
                { realtimeAudit::Guard guard;p.processBlock(buffer,midi); }
                const auto c=realtimeAudit::counts;++calls;
                totals.allocations+=c.allocations;totals.frees+=c.frees;totals.locks+=c.locks;totals.waits+=c.waits;totals.io+=c.io;
                check(c.allocations+c.frees+c.locks+c.waits+c.io==0,"callback made an audited non-realtime operation");
                for(int ch=0;ch<mainChannels;++ch)for(int i=0;i<n;++i) {
                    const auto a=buffer.getSample(ch,i),b=expected.getSample(ch,i);
                    check(std::isfinite(a),"nonfinite callback output");
                    const auto error=std::abs(double(a)-b);maxError=std::max(maxError,error);
                    check(error<1.e-7,"wrapper/reference mismatch");
                }
            }
            if(cases%2)p.removeSpectrumConsumer();
            p.releaseResources();
        }
    }
    // Real producer/consumer concurrency: main-thread spectrum and meter reads
    // while a separate audio thread runs audited callbacks. No GUI methods there.
    std::uint64_t concurrentViolations=0;
    {
        PonteMC2000AudioProcessor p;p.prepareToPlay(48000,512);p.addSpectrumConsumer();
        std::atomic<bool> done{false};
        std::thread producer([&] {
            juce::AudioBuffer<float> audio(2,512);juce::MidiBuffer events;
            for(int b=0;b<1024;++b) {
                for(int ch=0;ch<2;++ch)for(int i=0;i<512;++i)audio.setSample(ch,i,float(.2*std::sin((b*512+i)*.017)));
                { realtimeAudit::Guard guard;p.processBlock(audio,events); }
                auto c=realtimeAudit::counts;
                concurrentViolations+=c.allocations+c.frees+c.locks+c.waits+c.io;
                for(int ch=0;ch<2;++ch)for(int i=0;i<512;++i)if(!std::isfinite(audio.getSample(ch,i)))++concurrentViolations;
            }
            done.store(true,std::memory_order_release);
        });
        std::array<PonteMC2000AudioProcessor::SpectrumSample,257> spectrum;
        while(!done.load(std::memory_order_acquire)) {
            bool gap=false;p.popSpectrumSamples(spectrum.data(),257,gap);
            for(int b=0;b<4;++b) { p.getEngine().consumeBandMeter(b);p.getEngine().getStaticCurveParameters(b); }
            p.getEngine().consumeOutputMeterDb();std::this_thread::yield();
        }
        producer.join();p.removeSpectrumConsumer();
    }
    check(concurrentViolations==0,"concurrent callback violation");
    // A synthetic old model tag exercises the actual binary state entry point.
    // Real old DAW sessions still need host-level validation.
    {
        PonteMC2000AudioProcessor p;
        parameter(p,parameters::bandId(1,"ratio"),4);
        parameter(p,parameters::bandId(1,"tcMode"),2);
        p.editorWidth.store(1234);p.editorHeight.store(888);
        juce::MemoryBlock state;p.getStateInformation(state);
        auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize()));
        check(xml!=nullptr,"state XML missing");
        if(xml) {
            xml->setAttribute("dspModelVersion",5);
            juce::AudioProcessor::copyXmlToBinary(*xml,state);
            parameter(p,parameters::bandId(1,"ratio"),9);
            p.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
            check(p.editorWidth.load()==1234 && p.editorHeight.load()==888,"legacy-tagged state lost editor size");
            for(int rate:{48000,192000,44100}) {
                p.prepareToPlay(rate,128);juce::AudioBuffer<float> buffer(2,128);buffer.clear();
                { realtimeAudit::Guard g;p.processBlock(buffer,midi); }
                check(p.getEngine().getParameters().bands[1].ratio==4 && p.getEngine().getParameters().bands[1].tcMode==dsp::TCMode::automatic,"legacy state or repeated prepare lost parameters");
                const auto c=realtimeAudit::counts;
                check(c.allocations+c.frees+c.locks+c.waits+c.io==0,"callback after state restore not realtime");
                p.releaseResources();
            }
        }
    }
    std::ofstream out(path);
    out<<"{\n  \"cases\": "<<cases<<", \"callbacks\": "<<calls<<", \"installed_import_hooks\": "<<hooks
       <<",\n  \"allocations\": "<<totals.allocations<<", \"frees\": "<<totals.frees<<", \"locks\": "<<totals.locks
       <<", \"waits\": "<<totals.waits<<", \"io\": "<<totals.io<<",\n  \"max_audio_error\": "<<maxError
       <<", \"concurrent_callbacks\": 1024, \"concurrent_violations\": "<<concurrentViolations
       <<", \"failures\": "<<violations<<", \"passed\": "<<(ok?"true":"false")<<"\n}\n";
    std::cout<<"RT matrix "<<cases<<" cases, "<<calls<<" callbacks, error="<<maxError<<", violations="<<violations<<'\n';
    return ok?0:1;
}

inline int profile(const char* path) {
    // Single message thread; synthetic audio callbacks are timed separately.
    // Paint is explicitly software-rendered, so this is not a DAW/GPU benchmark.
    std::ofstream out(path);
    out<<"instances,scenario,repeat,elapsed_ms,thread_cpu_ms,private_bytes,wrapper_us,gui_ticks,gui_tick_us,fft_calls,fft_us,response_calls,response_us,knob_formats,layout_us,graph_paints,graph_paint_us,forced_frame_us\n";
    juce::MidiBuffer midi;
    for(int count:{1,4})for(int scenario=0;scenario<5;++scenario) {
        std::vector<std::unique_ptr<PonteMC2000AudioProcessor>> processors;
        std::vector<std::unique_ptr<PonteMC2000AudioProcessorEditor>> editors;
        for(int i=0;i<count;++i) {
            auto p=std::make_unique<PonteMC2000AudioProcessor>();p->prepareToPlay(48000,512);
            if(scenario) {
                auto e=std::make_unique<PonteMC2000AudioProcessorEditor>(*p);
                e->setTopLeftPosition(-20000,-20000);
                e->addToDesktop(juce::ComponentPeer::windowIsTemporary|juce::ComponentPeer::windowIgnoresMouseClicks|juce::ComponentPeer::windowIgnoresKeyPresses);
                e->setVisible(scenario!=1);editors.push_back(std::move(e));
            }
            processors.push_back(std::move(p));
        }
        juce::AudioBuffer<float> buffer(2,512);
        juce::Image canvas(juce::Image::ARGB,1600,1100,true);juce::Graphics graphics(canvas);
        for(int repeat=-1;repeat<5;++repeat) {
            mc2000probe::measurements={};mc2000probe::enabled=true;
            auto start=std::chrono::steady_clock::now();const auto cpu=threadCpuMs();
            double wrapper=0,forced=0;
            for(int frame=0;frame<20;++frame) {
                for(auto& p:processors)for(int block=0;block<3;++block) {
                    buffer.clear();
                    if(scenario>=3)for(int ch=0;ch<2;++ch)for(int i=0;i<512;++i)buffer.setSample(ch,i,float(.3*std::sin((frame*1536+block*512+i)*.041)));
                    const auto t=std::chrono::steady_clock::now();p->processBlock(buffer,midi);
                    wrapper+=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count();
                }
                if(scenario==4)for(auto& e:editors)e->setSize(frame%2?1100:1400,frame%2?738:950);
                juce::MessageManager::getInstance()->runDispatchLoopUntil(12);
                if(scenario>=2 && (scenario>=3 || frame==0))for(auto& e:editors) {
                    const auto t=std::chrono::steady_clock::now();e->paintEntireComponent(graphics,true);
                    forced+=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count();
                }
            }
            const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            const auto usedCpu=threadCpuMs()-cpu;mc2000probe::enabled=false;
            const auto& m=mc2000probe::measurements;
            if(repeat>=0)out<<count<<','<<scenario<<','<<repeat<<','<<elapsed<<','<<usedCpu<<','<<privateBytes()<<','<<wrapper<<','<<m[mc2000probe::guiTick].calls<<','<<m[mc2000probe::guiTick].microseconds<<','<<m[mc2000probe::fft].calls<<','<<m[mc2000probe::fft].microseconds<<','<<m[mc2000probe::response].calls<<','<<m[mc2000probe::response].microseconds<<','<<m[mc2000probe::knobText].calls<<','<<m[mc2000probe::layout].microseconds<<','<<m[mc2000probe::paint].calls<<','<<m[mc2000probe::paint].microseconds<<','<<forced<<'\n';
        }
        std::cout<<"Profile "<<count<<" instances, scenario "<<scenario<<" complete\n";
    }
    return 0;
}

inline int callbackProfile(const char* path) {
    std::ofstream out(path);
    out<<"rate,block,mode,consumer,repeat,median_us,p99_us,max_us,deadline_us,snapshot_us,fifo_us,dsp_us,median_cycles,wall_peak_cycles\n";
    juce::MidiBuffer midi;
    for(int rate:{48000,96000,192000})for(int block:{64,512,2048})
    for(int mode:{0,1,2})for(int consumer:{0,1}) {
        PonteMC2000AudioProcessor p;
        for(int b=0;b<4;++b) {
            parameter(p,parameters::bandId(b,"ratio"),4);
            parameter(p,parameters::bandId(b,"thresholdDb"),-30);
            parameter(p,parameters::bandId(b,"tcMode"),float(mode));
            parameter(p,parameters::bandId(b,"bite"),5);
        }
        p.prepareToPlay(rate,block);if(consumer)p.addSpectrumConsumer();
        juce::AudioBuffer<float> buffer(2,block);
        std::vector<double> times;times.reserve(64);
        std::vector<std::uint64_t> cycles;cycles.reserve(64);
        for(int repeat=-1;repeat<5;++repeat) {
            mc2000probe::measurements={};times.clear();cycles.clear();
            for(int n=0;n<64;++n) {
                for(int ch=0;ch<2;++ch)for(int i=0;i<block;++i)buffer.setSample(ch,i,float(.4*std::sin((n*block+i)*.042)));
                p.discardSpectrumSamples();mc2000probe::enabled=true;
                auto firstCycle=threadCycles();auto start=std::chrono::steady_clock::now();p.processBlock(buffer,midi);
                times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
                cycles.push_back(threadCycles()-firstCycle);
                mc2000probe::enabled=false;
            }
            const auto peakCycles=cycles[static_cast<size_t>(std::max_element(times.begin(),times.end())-times.begin())];
            std::sort(times.begin(),times.end());std::sort(cycles.begin(),cycles.end());const auto& m=mc2000probe::measurements;
            if(repeat>=0)out<<rate<<','<<block<<','<<mode<<','<<consumer<<','<<repeat<<','<<times[32]<<','<<times[62]<<','<<times[63]<<','<<1.e6*block/rate<<','<<m[mc2000probe::snapshot].microseconds/64<<','<<m[mc2000probe::fifo].microseconds/64<<','<<m[mc2000probe::dsp].microseconds/64<<','<<cycles[32]<<','<<peakCycles<<'\n';
        }
        if(consumer)p.removeSpectrumConsumer();
    }
    return 0;
}
}
