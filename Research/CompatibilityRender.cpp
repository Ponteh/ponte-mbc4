#include "DSP/MultiBandCompressor.h"
#include <fstream>
#include <numbers>
#include <vector>
using namespace pontedsp::mc2000::dsp;
int main(int argc,char** argv) {
    if(argc!=2)return 2;std::ofstream out(argv[1],std::ios::binary);
    for(int rate:{44100,48000,96000,192000})for(int bands:{2,3,4})for(int tc:{0,1,2}) {
        MultiBandCompressor engine;GlobalParameters p;p.numBands=bands;
        for(auto& b:p.bands){b.thresholdDb=-30;b.ratio=4;b.knee=5;b.bite=7;b.tcMode=static_cast<TCMode>(tc);}
        engine.setParameters(p);engine.prepare(rate,257,2);
        std::array<float,257> l,r,keyL,keyR;float* audio[]{l.data(),r.data()};const float* key[]{keyL.data(),keyR.data()};
        for(int offset=0;offset<rate/2;offset+=257) {
            const int n=std::min(257,rate/2-offset);
            for(int i=0;i<n;++i){const double t=double(offset+i)/rate;l[std::size_t(i)]=float(.4*std::sin(2*std::numbers::pi*315*t)+.1*std::sin(2*std::numbers::pi*7310*t));r[std::size_t(i)]=-.13f*l[std::size_t(i)];keyL[std::size_t(i)]=float(.3*std::sin(2*std::numbers::pi*523*t));keyR[std::size_t(i)]=.5f*keyL[std::size_t(i)];}
            if(offset>rate/4){p.inputGainDb=-3;p.outputGainDb=2;p.crossoverHz={130,1430,13000};p.bands[1].solo=true;p.bands[0].enabled=false;engine.setParameters(p);}
            engine.process(audio,2,tc==2?key:nullptr,tc==2?2:0,n);
            out.write(reinterpret_cast<const char*>(l.data()),n*sizeof(float));out.write(reinterpret_cast<const char*>(r.data()),n*sizeof(float));
        }
    }
    return out.good()?0:1;
}
