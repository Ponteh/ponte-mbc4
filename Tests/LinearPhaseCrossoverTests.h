#include <chrono>
namespace {
void testMeasuredLinearPhaseResponse() {
    using namespace pontedsp::mc2000::dsp;
    for(double rate:{44100.,48000.,96000.,192000.}) {
        LinearPhaseCrossover x;x.prepare(rate,{20,21,20000});
        const auto h=LinearPhaseCrossover::designLowPass(rate,20,x.tapCount());
        LinearPhaseCrossover::Responses response;std::array<double,3> f;unsigned version=0;
        expect(x.copyResponse(response,f,version),"Prepared FIR publishes a coherent response snapshot");
        std::array<int,6> points{0,15,45,90,140,180};
        std::array<std::complex<double>,6> oscillator,step;
        std::array<std::array<std::complex<double>,6>,4> measured{};
        for(std::size_t i=0;i<6;++i){const double frequency=20*std::pow(std::min(20000.,rate*.5)/20,double(points[i])/180);oscillator[i]=1;step[i]=std::polar(1.,-2*std::numbers::pi*frequency/rate);}
        double convolutionError=0;std::array<std::array<double,4>,4> out;
        for(int n=0;n<x.tapCount()+2*x.partitionSize()+1;++n){
            x.processFrame({n==0?1.:0.,0,0,0},1,4,out);
            const int sample=n-2*x.partitionSize();
            const double expected=sample>=0&&sample<int(h.size())?h[std::size_t(sample)]:0;
            convolutionError=std::max(convolutionError,std::abs(out[0][0]-expected));
            for(std::size_t i=0;i<6;++i){for(std::size_t b=0;b<4;++b)measured[b][i]+=out[0][b]*oscillator[i];oscillator[i]*=step[i];}
        }
        expect(convolutionError<1.e-9,"Partitioned FIR matches independent direct impulse convolution");
        expect(x.getSchedulingOverruns()==0,"FIR operation budget does not overrun");
        for(std::size_t i=0;i<6;++i){
            const double frequency=20*std::pow(std::min(20000.,rate*.5)/20,double(points[i])/180);
            const auto phase=std::polar(1.,2*std::numbers::pi*frequency*x.latencySamples()/rate);
            for(std::size_t b=0;b<4;++b){
                const double upper=b==3?1:response[b][std::size_t(points[i])],lower=b==0?0:response[b-1][std::size_t(points[i])];
                const auto actual=measured[b][i]*phase;
                expect(std::abs(actual.imag())<1.e-8,"Measured FIR bands share the declared linear phase/group delay");
                if(std::abs(actual)>1.e-4)expectNear(gainToDecibels(std::abs(upper-lower)),gainToDecibels(std::abs(actual)),.1,"Published FIR curve matches independently measured magnitude");
                else { if(std::abs((upper-lower)-actual.real())>=2.e-6) std::cerr << "FIR floor rate=" << rate << " point=" << points[i] << " band=" << b << " predicted=" << upper-lower << " actual=" << actual.real() << '\n'; expect(std::abs((upper-lower)-actual.real())<2.e-6,"FIR graphical floor uses absolute error"); }
            }
        }
    }
}
void testLinearPhaseKernelUpdates() {
    using namespace pontedsp::mc2000::dsp;
    LinearPhaseCrossover x;x.prepare(48000,{100,1000,10000});
    std::array<std::array<double,4>,4> out;LinearPhaseCrossover::Responses response;std::array<double,3> f;unsigned version=0;
    int n=0;double error=0,previous=0,maxStep=0;
    const auto process=[&](int count){for(int i=0;i<count;++i,++n){const double input=.5*std::sin(2*std::numbers::pi*63*n/48000);x.processFrame({input,-input,0,0},3,4,out);double sum=0;for(double v:out[0])sum+=v;const double expected=n<x.latencySamples()?0:.5*std::sin(2*std::numbers::pi*63*(n-x.latencySamples())/48000);error=std::max(error,std::abs(sum-expected));maxStep=std::max(maxStep,std::abs(out[0][0]-previous));previous=out[0][0];}};
    process(24000);x.requestFrequencies({500,1000,10000});x.requestFrequencies({200,1000,10000});x.requestFrequencies({20,1000,10000});
    bool applied=false;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(std::chrono::steady_clock::now()<deadline){process(512);if(x.copyResponse(response,f,version)&&f[0]==20){applied=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    process(4*x.partitionSize()+2000);
    expect(applied,"Worker coalesces rapid requests and publishes the latest kernel");
    expect(error<=1.e-6,"Kernel transitions preserve the delayed unity reconstruction");
    expect(maxStep<.01,"Warm-history kernel fade keeps the 63 Hz transient step below 0.01 full scale");
    expect(x.getSchedulingOverruns()==0,"Kernel transition fits its larger bounded operation budget");
}
}
