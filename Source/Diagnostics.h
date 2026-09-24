#pragma once
// Compiled out of the VST3. Measurements belong to the technical test executable.
#if defined(MC2000_TECHNICAL_TESTS)
#include <array>
#include <chrono>
#include <cstdint>
namespace mc2000probe {
enum Category { snapshot, fifo, dsp, guiTick, fft, response, knobText, layout, paint, count };
struct Measurement { std::uint64_t calls {}; double microseconds {}, maximumUs {}; };
inline thread_local bool enabled = false;
inline thread_local std::array<Measurement,count> measurements {};
struct Scope {
    Category category; bool active;
    std::chrono::steady_clock::time_point start;
    explicit Scope(Category c) : category(c), active(enabled) { if(active) start=std::chrono::steady_clock::now(); }
    ~Scope() { if(active) { auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count(); auto& m=measurements[category]; ++m.calls; m.microseconds+=us; if(us>m.maximumUs)m.maximumUs=us; } }
};
}
#define MC2000_MEASURE(category) mc2000probe::Scope mc2000ProbeScope(mc2000probe::category)
#else
#define MC2000_MEASURE(category) ((void)0)
#endif
