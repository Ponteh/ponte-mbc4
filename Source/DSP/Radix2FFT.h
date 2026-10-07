#pragma once
#include "ComplexArithmetic.h"
#include <algorithm>
#include <complex>
#include <numbers>
#include <vector>
namespace pontedsp::mc2000::dsp {
// Double precision FFT with immutable tables prepared outside the callback.
class Radix2FFT final {
public:
    void prepare(int size) {
        n = size;
        reversed.resize(static_cast<std::size_t>(n));
        roots.resize(static_cast<std::size_t>(n / 2));
        for (int i = 0, j = 0; i < n; ++i) {
            reversed[static_cast<std::size_t>(i)] = j;
            int bit = n >> 1;
            while (bit && (j & bit)) { j ^= bit; bit >>= 1; }
            j ^= bit;
        }
        for (int i = 0; i < n / 2; ++i)
            roots[static_cast<std::size_t>(i)] = std::polar(1.0, -2.0 * std::numbers::pi * i / n);
    }
    struct Cursor { int phase {}, index {}, length {2}; bool inverse {}; };
    // The same FFT, in bounded portions. Each operation is a swap, butterfly,
    // or normalization; the caller spreads it over the next input partition.
    int advance(std::complex<double>* data, Cursor& cursor, int budget) const noexcept {
        const int initial = budget;
        while (budget > 0 && cursor.phase < 3) {
            if (cursor.phase == 0) {
                const int count = std::min(budget, n - cursor.index);
                for (int i = cursor.index; i < cursor.index + count; ++i)
                    if (i < reversed[static_cast<std::size_t>(i)]) std::swap(data[i], data[reversed[static_cast<std::size_t>(i)]]);
                cursor.index += count; budget -= count;
                if (cursor.index == n) { cursor.index = 0; cursor.phase = 1; }
            } else if (cursor.phase == 1) {
                const int half = cursor.length / 2, step = n / cursor.length;
                const int count = std::min(budget, n / 2 - cursor.index);
                for (int i = cursor.index; i < cursor.index + count; ++i) {
                    const int j = i & (half - 1), at = 2 * i - j;
                    const auto root = roots[static_cast<std::size_t>(j * step)];
                    complexButterfly(data[at],data[at + half],root,cursor.inverse);
                }
                cursor.index += count; budget -= count;
                if (cursor.index == n / 2) {
                    cursor.index = 0; cursor.length *= 2;
                    if (cursor.length > n) cursor.phase = cursor.inverse ? 2 : 3;
                }
            } else {
                const int count = std::min(budget, n - cursor.index);
                for (int i = cursor.index; i < cursor.index + count; ++i) data[i] /= n;
                cursor.index += count; budget -= count;
                if (cursor.index == n) cursor.phase = 3;
            }
        }
        return initial - budget;
    }
    void transform(std::complex<double>* data, bool inverse) const noexcept {
        for (int i = 0; i < n; ++i)
            if (i < reversed[static_cast<std::size_t>(i)]) std::swap(data[i], data[reversed[static_cast<std::size_t>(i)]]);
        for (int length = 2; length <= n; length *= 2) {
            const int half = length / 2, step = n / length;
            for (int start = 0; start < n; start += length)
                for (int j = 0; j < half; ++j) {
                    const auto root = roots[static_cast<std::size_t>(j * step)];
                    complexButterfly(data[start + j],data[start + j + half],root,inverse);
                }
        }
        if (inverse) for (int i = 0; i < n; ++i) data[i] /= n;
    }
private:
    int n {};
    std::vector<int> reversed;
    std::vector<std::complex<double>> roots;
};
}
