#pragma once
#include <random>
namespace {
void testMeasuredIirCrossoverResponse() {
    using namespace pontedsp::mc2000::dsp;
    for (double rate : {44100.,48000.,96000.,192000.})
    for (int bands : {2,3,4}) {
        CrossoverNetwork network; network.prepare(rate,1); network.setBandCount(bands);
        network.setFrequencies({20,21,20000});
        const std::array<double,6> frequencies {20,21,100,1000,18000,rate*.49};
        std::array<std::array<std::complex<double>,6>,4> measured {};
        std::array<std::complex<double>,6> oscillator, increment;
        for(std::size_t i=0;i<6;++i) {oscillator[i]=1;increment[i]=std::polar(1.,-2*std::numbers::pi*frequencies[i]/rate);}
        for(int n=0;n<int(rate*1.5);++n) {
            std::array<double,4> split;network.processSample(0,n==0?1.:0.,split);
            for(std::size_t i=0;i<6;++i) {for(int b=0;b<bands;++b) {measured[std::size_t(b)][i]+=split[std::size_t(b)]*oscillator[i];}oscillator[i]*=increment[i];}
        }
        for(std::size_t i=0;i<6;++i) {
            std::complex<double> actualSum {}, expectedSum {};
            for(int b=0;b<bands;++b) {
                const auto predicted=network.getBandResponse(b,frequencies[i]);
                const auto actual=measured[std::size_t(b)][i];
                if(std::abs(actual)>1.e-4) expectNear(gainToDecibels(std::abs(predicted)),gainToDecibels(std::abs(actual)),.1,"IIR curve matches independently measured impulse");
                else expect(std::abs(predicted-actual)<1.e-8,"IIR response below floor uses absolute error");
                actualSum+=actual;expectedSum+=predicted;
            }
            expect(std::abs(actualSum-expectedSum)<1.e-7,"IIR complex reconstruction matches measured phase/sum");
        }
    }
}
void testLinearPhaseReconstruction() {
    using namespace pontedsp::mc2000::dsp;
    std::mt19937 random(2525);std::uniform_real_distribution<float> distribution(-1,1);
    for(double rate:{44100.,48000.,96000.,192000.}) {
        GlobalParameters p;p.crossoverMode=CrossoverMode::linearPhase;p.crossoverHz={20,21,22};
        MultiBandCompressor engine;engine.setParameters(p);engine.prepare(rate,64,2);
        expect(engine.getLatencySamples()>0,"Linear Phase has actual FIR latency");
        const int latency=engine.getLatencySamples();
        for(int bands:{2,3,4}) for(auto mode:{ChannelMode::stereo,ChannelMode::dualMono}) {
            p.numBands=bands;p.channelMode=mode;engine.setParameters(p);engine.reset();
            const int count=latency+4097;
            std::vector<float> left(std::size_t(count),0),right(std::size_t(count),0),originalL(left),originalR(right);
            for(int i=0;i<4097;++i) {left[std::size_t(i)]=originalL[std::size_t(i)]=distribution(random);right[std::size_t(i)]=originalR[std::size_t(i)]=distribution(random);}
            for(int offset=0,step=0;offset<count;++step) {
                constexpr std::array<int,6> sizes{1,17,64,512,2048,4097};const int n=std::min(sizes[std::size_t(step%6)],count-offset);
                float* audio[]{left.data()+offset,right.data()+offset};engine.process(audio,2,n);offset+=n;
            }
            double error=0;for(int i=0;i<count;++i) {
                const float l=i<latency?0:originalL[std::size_t(i-latency)], r=i<latency?0:originalR[std::size_t(i-latency)];
                error=std::max({error,std::abs(double(left[std::size_t(i)])-l),std::abs(double(right[std::size_t(i)])-r)});
            }
            expect(engine.getLinearSchedulingOverruns()==0,"Bounded FIR schedule completes before each output partition");
            expect(error<=1.e-6,"FIR sum is delayed input for bands/modes/irregular and oversized blocks");
        }
    }
}
void testIndependentDualMonoDynamics() {
    using namespace pontedsp::mc2000::dsp;
    for(auto crossover:{CrossoverMode::iir,CrossoverMode::linearPhase}) {
        GlobalParameters p;p.crossoverMode=crossover;p.channelMode=ChannelMode::dualMono;
        MultiBandCompressor stereo,leftMono,rightMono;
        for(auto* engine:{&stereo,&leftMono,&rightMono}) {engine->setNapEnabled(false);engine->setParameters(p);engine->prepare(48000,64,engine==&stereo?2:1);}
        for(int bands:{2,3,4}) for(int keyChannels:{0,1,2}) for(auto tc:{TCMode::type1,TCMode::type2,TCMode::automatic}) {
            p.numBands=bands;for(auto& band:p.bands){band.thresholdDb=-36;band.ratio=4;band.bite=tc==TCMode::type1?1:10;band.tcMode=tc;}
            for(auto* engine:{&stereo,&leftMono,&rightMono}){engine->setParameters(p);engine->reset();}
            double error=0;constexpr int block=257;
            std::array<float,block> l,r,lm,rm,kl,kr;
            for(int offset=0;offset<24000;offset+=block) {
                const int count=std::min(block,24000-offset);
                for(int i=0;i<count;++i) {
                    const double t=double(offset+i)/48000;
                    const float a=float(.7*std::sin(2*std::numbers::pi*315*t)), b=float(.006*std::sin(2*std::numbers::pi*2230*t));
                    l[std::size_t(i)]=lm[std::size_t(i)]=a;r[std::size_t(i)]=rm[std::size_t(i)]=b;
                    kl[std::size_t(i)]=float(.5*std::sin(2*std::numbers::pi*700*t));kr[std::size_t(i)]=float(.002*std::sin(2*std::numbers::pi*700*t));
                }
                float* program[]{l.data(),r.data()};float* monoL[]{lm.data()};float* monoR[]{rm.data()};
                const float* key[]{kl.data(),kr.data()};const float* keyL[]{kl.data()};const float* keyR[]{keyChannels==1?kl.data():kr.data()};
                stereo.process(program,2,keyChannels?key:nullptr,keyChannels,count);
                leftMono.process(monoL,1,keyChannels?keyL:nullptr,keyChannels?1:0,count);
                rightMono.process(monoR,1,keyChannels?keyR:nullptr,keyChannels?1:0,count);
                for(int i=0;i<count;++i)error=std::max({error,std::abs(double(l[std::size_t(i)])-lm[std::size_t(i)]),std::abs(double(r[std::size_t(i)])-rm[std::size_t(i)])});
            }
            expect(error<2.e-6,"Each Dual Mono channel equals its independent mono compressor, including mapped key and BITE/TC");
            for(int band=0;band<bands;++band) {
                const auto channelMeter=stereo.getBandMeter(band), monoLeft=leftMono.getBandMeter(band), monoRight=rightMono.getBandMeter(band);
                expectNear(channelMeter.channelInputDb[0],monoLeft.inputDb,1.e-4,"Left IN equals the independent mono detector meter");
                expectNear(channelMeter.channelInputDb[1],monoRight.inputDb,1.e-4,"Right IN equals the independent mono detector meter");
                expectNear(channelMeter.channelOutputDb[0],monoLeft.outputDb,1.e-4,"Left OUT equals the independent mono output meter");
                expectNear(channelMeter.channelOutputDb[1],monoRight.outputDb,1.e-4,"Right OUT equals the independent mono output meter");
                expect(monoLeft.channelInputDb[1]==-100 && monoLeft.channelOutputDb[1]==-100 && monoLeft.channelGainReductionDb[1]==0,"Mono layouts leave the absent right channel empty");
            }
            const auto meter=stereo.getBandMeter(1);expectNear(meter.gainReductionDb,std::max(meter.channelGainReductionDb[0],meter.channelGainReductionDb[1]),1.e-5,"Dual Mono aggregate GR is max(L,R)");
        }
    }
}
void testChannelModeTransitionAndNap() {
    using namespace pontedsp::mc2000::dsp;
    MultiBandCompressor e;GlobalParameters p;p.channelMode=ChannelMode::dualMono;
    for(auto& b:p.bands){b.ratio=10;b.thresholdDb=-36;b.attackMs=.25;}
    e.setParameters(p);e.prepare(48000,512,2);
    std::array<float,512> l{},r{};float* audio[]{l.data(),r.data()};
    for(int n=0;n<20;++n){l.fill(.01f);r.fill(.6f);e.process(audio,2,512);}
    p.channelMode=ChannelMode::stereo;e.setParameters(p);
    for(int n=0;n<1100;++n){l.fill(0);r.fill(0);e.process(audio,2,512);}
    expect(e.getActivity()==MultiBandCompressor::Activity::sleeping,"Inactive right detector cannot prevent Stereo nap after mode change");
    p.channelMode=ChannelMode::dualMono;e.setParameters(p);l.fill(.02f);r.fill(.7f);e.process(audio,2,512);
    expect(e.getActivity()!=MultiBandCompressor::Activity::sleeping,"Channel change and right-only audio wake nap");
}
void testRawSilenceDetection() {
    using namespace pontedsp::mc2000::dsp;
    expect(exactlyZero(0.0f) && exactlyZero(-0.0f) && exactlyZero(0.0) && exactlyZero(-0.0),"Both IEEE zero signs are silence");
    const std::uint32_t floatBits=1;const std::uint64_t doubleBits=1;
    float tinyFloat;double tinyDouble;std::memcpy(&tinyFloat,&floatBits,sizeof(tinyFloat));std::memcpy(&tinyDouble,&doubleBits,sizeof(tinyDouble));
    expect(!exactlyZero(tinyFloat) && !exactlyZero(tinyDouble),"Raw subnormal bits remain nonzero under the callback denormal guard");
    MultiBandCompressor engine;engine.prepare(48000,64,2);
    std::array<float,64> left{},right,key;right.fill(-0.0f);key.fill(-0.0f);
    float* audio[]{left.data(),right.data()};const float* detector[]{key.data()};
    engine.process(audio,2,detector,1,64);
    expect(engine.getActivity()==MultiBandCompressor::Activity::sleeping,"Signed zero program/key permits nap");
    right[0]=tinyFloat;engine.process(audio,2,64);
    expect(engine.getActivity()!=MultiBandCompressor::Activity::sleeping,"Raw right-channel subnormal wakes nap even with DAZ/FTZ");
}
void testActiveCoefficientPublication() {
    using namespace pontedsp::mc2000::dsp;
    MultiBandCompressor e;e.prepare(48000,64,2);e.setNapEnabled(false);
    CrossoverResponse before,after;unsigned v0=0,v1=0;
    expect(e.copyIirResponse(before,v0),"Prepared IIR publishes actual coefficients");
    auto p=e.getParameters();p.crossoverHz={500,2000,8000};e.setParameters(p);
    std::array<float,1> l{.2f},r{.1f};float* audio[]{l.data(),r.data()};e.process(audio,2,1);
    expect(e.copyIirResponse(after,v1)&&v1!=v0&&after.frequencies[0]>100&&after.frequencies[0]<500,"Coefficient publication tracks active smoothing without changing its audio law");
    expect(std::abs(after.bandResponse(0,130)-before.bandResponse(0,130))>1.e-6,"Applied coefficient change updates the complex response deterministically");
}
void testCrossoverAndChannelModes() {
    testActiveCoefficientPublication();
    testRawSilenceDetection();
    testMeasuredIirCrossoverResponse();testLinearPhaseReconstruction();testIndependentDualMonoDynamics();testChannelModeTransitionAndNap();
}
}
