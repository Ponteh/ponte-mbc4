#pragma once
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE__)
#include <xmmintrin.h>
struct ResearchNoDenormals
{
    unsigned int saved { _mm_getcsr() };
    ResearchNoDenormals() { _mm_setcsr(saved | 0x8040u); }
    ~ResearchNoDenormals() { _mm_setcsr(saved); }
};
#else
struct ResearchNoDenormals {};
#endif
