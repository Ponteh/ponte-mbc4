#pragma once
#include "Biquad.h"
#include <array>
#include <algorithm>
namespace pontedsp::mc2000::dsp {
// Immutable UI copy of the exact active audio coefficients, including all-pass sections.
struct CrossoverResponse {
    double sampleRate {48000};
    int bands {4};
    std::array<double,3> frequencies {100,1000,10000};
    std::array<std::array<Biquad::Coefficients,4>,6> sections;
    std::complex<double> bandResponse(int band,double frequency) const noexcept {
        if(band<0 || band>=bands || !std::isfinite(frequency)) return {};
        const auto f=std::clamp(frequency,0.0,sampleRate*.5);
        const auto low=[&](int i){const auto& c=sections[std::size_t(i)];return Biquad::response(c[0],f,sampleRate)*Biquad::response(c[1],f,sampleRate);};
        const auto high=[&](int i){const auto& c=sections[std::size_t(i)];return Biquad::response(c[2],f,sampleRate)*Biquad::response(c[3],f,sampleRate);};
        const auto ap=[&](int i){return low(i+3)+high(i+3);};
        if(bands==2) return band==0?low(0):high(0);
        if(bands==3) {
            if(band==0)return low(1)*low(0);
            if(band==1)return high(0)*low(1);
            return high(1)*ap(0);
        }
        if(band==0)return low(2)*low(1)*low(0);
        if(band==1)return high(0)*low(1)*low(2);
        if(band==2)return high(1)*low(2)*ap(0);
        return high(2)*ap(1)*ap(2);
    }
};
}