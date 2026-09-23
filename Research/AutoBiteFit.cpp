// Research-only candidate evaluator, called by fit_auto_bite_2026_09_23.py.
#include <cmath>
#include <algorithm>
#if defined(_WIN32)
#define FIT_EXPORT __declspec(dllexport)
#else
#define FIT_EXPORT __attribute__((visibility("default")))
#endif
extern "C" FIT_EXPORT void evaluate(const double* target, double* output, int count,
                                    double rate, double attackMs, int domain)
{
    const double attack=std::exp(-1.0/(rate*attackMs*.001));
    const double release=std::exp(-1.0/(rate*.102));
    double q=1.0,gr=0.0,smooth=0.0,smooth2=0.0;
    const double fast=std::exp(-1.0/(rate*.00002));
    for(int i=0;i<count;++i)
    {
        const double g=target[i];
        if(domain==0)
        {
            const double goal=std::pow(10.0,g/10.0);
            q=goal>q ? attack*q+(1.0-attack)*goal : 1.0+release*(q-1.0);
            gr=10*std::log10(q);
        }
        else if(domain==1)
        {
            gr=g>gr ? attack*gr+(1.0-attack)*g
                : 10*std::log10(1+(std::pow(10.0,gr/10.0)-1)*release);
        }
        else
        {
            const double goal=std::pow(10.0,g/10.0);
            q=goal>q ? fast*q+(1.0-fast)*goal : 1.0+release*(q-1.0);
            const double normal=10*std::log10(q);
            smooth=normal>smooth ? attack*smooth+(1-attack)*normal : normal;
            smooth2=smooth>smooth2 ? attack*smooth2+(1-attack)*smooth : smooth;
            gr=domain==2 ? smooth : smooth2;
        }
        output[i]=gr;
    }
}
