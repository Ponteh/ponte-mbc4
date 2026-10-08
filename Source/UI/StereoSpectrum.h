#pragma once

#include "MeterBallistics.h"
#include <juce_dsp/juce_dsp.h>
#include "../DSP/Db.h"
#include "../Diagnostics.h"
#include <array>
#include <cstdint>

namespace pontedsp::mc2000::ui {

// Message-thread stereo analysis. All storage is fixed; FFT/window/workspace
// are shared by the program and key traces, and never used in processBlock().
class StereoSpectrum final
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int bins = fftSize / 2;
    using Levels = std::array<float, bins>;
    using Response = std::array<double, bins>;
    using Workspace = std::array<float, fftSize * 2>;

    StereoSpectrum() { clear(); }

    void clear() noexcept
    {
        input = {};
        inputCount = 0;
        ready = false;
        withoutSamplesSeconds = 0;
        latest.fill(-100);
        peaks.fill(-100);
        levels.fill(-100);
        newFrame = false; dirty = true;
        for (auto& meter : ballistics) meter.reset();
    }

    void discardPartialWindow() noexcept { inputCount = 0; }
    void beginUpdate() noexcept { peaks.fill(-100); newFrame = false; }

    void push(float left, float right, juce::dsp::FFT& fft,
              const juce::dsp::WindowingFunction<float>& window, Workspace& work) noexcept
    {
        input[0][std::size_t(inputCount)] = left;
        input[1][std::size_t(inputCount)] = right;
        if (++inputCount < fftSize) return;
        for (std::size_t channel = 0; channel < input.size(); ++channel)
        {
            // Mono/identical stereo windows need one transform. Opposite-phase
            // channels remain independent, so their displayed levels cannot cancel.
            if (channel == 1 && input[1] == input[0]) continue;
            if (std::none_of(input[channel].begin(), input[channel].end(),
                [](float value) { return !pontedsp::mc2000::dsp::exactlyZero(value); })) continue;
            work.fill(0);
            std::copy(input[channel].begin(), input[channel].end(), work.begin());
            window.multiplyWithWindowingTable(work.data(), fftSize);
            { MC2000_MEASURE(fft); fft.performFrequencyOnlyForwardTransform(work.data()); }
            ++transforms;
            for (int bin = 0; bin < bins; ++bin)
                peaks[std::size_t(bin)] = std::max(peaks[std::size_t(bin)],
                    juce::Decibels::gainToDecibels(work[std::size_t(bin)] * (2.0f / fftSize), -100.0f));
        }
        for (auto& channel : input)
            std::copy(channel.begin() + fftSize / 2, channel.end(), channel.begin());
        inputCount = fftSize / 2;
        dirty = dirty || !ready;
        ready = newFrame = true;
    }

    bool finishUpdate(double elapsedSeconds, int samplesReceived, double sampleRate,
                      const Response& response) noexcept
    {
        if (samplesReceived > 0) withoutSamplesSeconds = 0;
        if (newFrame) latest = peaks;
        else if (samplesReceived == 0)
        {
            latest.fill(-100);
            withoutSamplesSeconds += elapsedSeconds;
            if (withoutSamplesSeconds > std::max(.1, 2.0 * fftSize / sampleRate))
                discardPartialWindow();
        }
        bool changed = dirty;
        dirty = false;
        for (std::size_t bin = 0; bin < levels.size(); ++bin)
        {
            const float next = float(ballistics[bin].update(latest[bin] + response[bin], elapsedSeconds));
            changed = changed || next != levels[bin];
            levels[bin] = next;
        }
        return changed;
    }

    const Levels& displayed() const noexcept { return levels; }
    bool isReady() const noexcept { return ready; }
    std::uint64_t performedTransforms() const noexcept { return transforms; }

private:
    std::array<std::array<float, fftSize>, 2> input {};
    std::array<pontedsp::gui::LevelMeterBallistics, bins> ballistics;
    Levels latest {}, peaks {}, levels {};
    int inputCount {};
    bool ready {}, newFrame {}, dirty {};
    double withoutSamplesSeconds {};
    std::uint64_t transforms {};
};
} // namespace pontedsp::mc2000::ui
