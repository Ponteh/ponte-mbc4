#pragma once
#include <complex>
#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define PONTE_COMPLEX_SSE2 1
#endif
namespace pontedsp::mc2000::dsp {
// FIR/FFT operands are finite. std::complex<double> guarantees interleaved
// real/imaginary double storage; SSE2 is part of the x86_64 baseline ISA.
#if defined(PONTE_COMPLEX_SSE2)
inline __m128d complexProduct(__m128d a, __m128d b) noexcept {
    const auto real = _mm_mul_pd(_mm_unpacklo_pd(a,a),b);
    const auto imag = _mm_mul_pd(_mm_unpackhi_pd(a,a),_mm_shuffle_pd(b,b,1));
    return _mm_add_pd(real,_mm_xor_pd(imag,_mm_set_pd(0.0,-0.0)));
}
#endif
inline void complexMultiplyAccumulate(std::complex<double>& out,
                                      const std::complex<double>& a,
                                      const std::complex<double>& b) noexcept {
#if defined(PONTE_COMPLEX_SSE2)
    const auto product = complexProduct(_mm_loadu_pd(reinterpret_cast<const double*>(&a)),
                                        _mm_loadu_pd(reinterpret_cast<const double*>(&b)));
    _mm_storeu_pd(reinterpret_cast<double*>(&out),_mm_add_pd(_mm_loadu_pd(reinterpret_cast<const double*>(&out)),product));
#else
    out += std::complex<double>{a.real()*b.real()-a.imag()*b.imag(),a.real()*b.imag()+a.imag()*b.real()};
#endif
}
inline void complexButterfly(std::complex<double>& left, std::complex<double>& right,
                             const std::complex<double>& root, bool inverse) noexcept {
#if defined(PONTE_COMPLEX_SSE2)
    const auto a = _mm_loadu_pd(reinterpret_cast<const double*>(&left));
    auto factor = _mm_loadu_pd(reinterpret_cast<const double*>(&root));
    if(inverse) factor = _mm_xor_pd(factor,_mm_set_pd(-0.0,0.0));
    const auto b = complexProduct(_mm_loadu_pd(reinterpret_cast<const double*>(&right)),factor);
    _mm_storeu_pd(reinterpret_cast<double*>(&left),_mm_add_pd(a,b));
    _mm_storeu_pd(reinterpret_cast<double*>(&right),_mm_sub_pd(a,b));
#else
    const auto a=left;
    const double imaginary=inverse?-root.imag():root.imag();
    const std::complex<double> b{right.real()*root.real()-right.imag()*imaginary,right.real()*imaginary+right.imag()*root.real()};
    left=a+b;right=a-b;
#endif
}
}
#undef PONTE_COMPLEX_SSE2
