#include "DSP/LinearPhaseCrossover.h"
#include <chrono>
#include <iostream>
#include <random>
using namespace pontedsp::mc2000::dsp;
int main() {
    for (double rate : {44100.,48000.,96000.,192000.}) {
        LinearPhaseCrossover x; x.prepare(rate,{20,21,22});
        const auto h=LinearPhaseCrossover::designLowPass(rate,20,x.tapCount());
        double ripple=0,stop=0,symmetry=0;
        for(double f=0;f<=10;f+=.25) ripple=std::max(ripple,std::abs(20*std::log10(std::abs(LinearPhaseCrossover::realResponse(h,rate,f)))));
        for(double f=30;f<=100;f+=.25) stop=std::max(stop,std::abs(LinearPhaseCrossover::realResponse(h,rate,f)));
        for(std::size_t i=0;i<h.size();++i) symmetry=std::max(symmetry,std::abs(h[i]-h[h.size()-1-i]));
        double err=0, bandErr=0, maxHopUs=0; const int length=x.tapCount()+x.latencySamples()+2*x.partitionSize();
        std::array<std::array<double,4>,4> out;
        auto start=std::chrono::steady_clock::now();
        for(int i=0;i<length;++i) {
            const auto tick=std::chrono::steady_clock::now();
            x.processFrame({i==0?1.:0.,0,0,0},1,4,out);
            const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-tick).count(); maxHopUs=std::max(maxHopUs,us);
            double sum=0;for(auto v:out[0])sum+=v;
            err=std::max(err,std::abs(sum-(i==x.latencySamples()?1.:0.)));
            const int k=i-2*x.partitionSize();
            bandErr=std::max(bandErr,std::abs(out[0][0]-(k>=0&&k<int(h.size())?h[std::size_t(k)]:0.)));
        }
        auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        // Independent direct FIR timing, full dot product, no FFT/formula reuse.
        std::vector<double> directInput(h.size()+512);
        for(std::size_t i=0;i<directInput.size();++i) directInput[i]=std::sin(double(i)*.039);
        volatile double checksum=0; const auto directStart=std::chrono::steady_clock::now();
        for(int n=0;n<512;++n) {double sum=0;for(std::size_t i=0;i<h.size();++i)sum+=h[i]*directInput[i+std::size_t(n)];checksum=checksum+sum;}
        const auto direct=std::chrono::duration<double>(std::chrono::steady_clock::now()-directStart).count()/512;
        std::cout<<rate<<" taps="<<x.tapCount()<<" hop="<<x.partitionSize()<<" latency="<<x.latencySamples()<<" ripple="<<ripple<<" stop="<<20*std::log10(stop)<<" symmetry="<<symmetry<<" sum_error="<<err<<" direct_error="<<bandErr<<" realtime_ratio="<<elapsed/(length/rate)<<" peak_frame_us="<<maxHopUs<<" scheduling_overruns="<<x.getSchedulingOverruns()<<" direct_sample_us="<<direct*1.e6<<"\n";
        if(x.getSchedulingOverruns()!=0 || err>1.e-6 || bandErr>1.e-9 || ripple>.1 || stop>.001 || symmetry!=0) return 1;
    }
}
