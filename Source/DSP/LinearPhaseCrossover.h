#pragma once
#include "Radix2FFT.h"
#include <array>
#include <atomic>
#include <thread>
#include <vector>
namespace pontedsp::mc2000::dsp {
// Cumulative symmetric FIRs, overlap-save partitions, one common delay.
// prepare/destruction require the audio callback to be stopped.
class LinearPhaseCrossover final {
public:
    static constexpr int curvePoints = 181, spectrumPoints = 1024;
    static constexpr int responsePoints = curvePoints + spectrumPoints;
    using Responses = std::array<std::array<double, responsePoints>, 3>;
    ~LinearPhaseCrossover();
    void prepare(double rate, const std::array<double, 3>& frequencies);
    void reset() noexcept;
    void stopWorker();
    void requestFrequencies(const std::array<double, 3>& frequencies) noexcept;
    void processFrame(const std::array<double, 4>& input, int laneMask, int bands,
                      std::array<std::array<double, 4>, 4>& output) noexcept;
    bool isQuiet() const noexcept;
    bool isTransitioning() const noexcept { return publishedTransition.load(std::memory_order_relaxed); }
    int latencySamples() const noexcept { return delay + 2 * hop; }
    int tapCount() const noexcept { return taps; }
    int partitionSize() const noexcept { return hop; }
    unsigned getResponseVersion() const noexcept {
        const int index = publishedActive.load(std::memory_order_acquire);
        const unsigned version = banks[static_cast<std::size_t>(index)].responseVersion.load(std::memory_order_seq_cst);
        return (version == 0 || (version & 1u)) ? 0 : version * 4 + static_cast<unsigned>(index);
    }
    unsigned getSchedulingOverruns() const noexcept { return schedulingOverruns.load(std::memory_order_relaxed); }
    bool copyResponse(Responses&, std::array<double, 3>&, unsigned& version) const noexcept;
    static int profileTaps(double rate) noexcept;
    static std::vector<double> designLowPass(double rate, double frequency, int length);
    static double realResponse(const std::vector<double>& kernel, double rate, double frequency) noexcept;
private:
    using Complex = std::complex<double>;
    struct Bank {
        std::array<std::vector<Complex>, 3> spectra;
        std::array<std::atomic<double>, 3> frequencies {};
        std::array<std::array<std::atomic<double>, responsePoints>, 3> responses {};
        std::atomic<unsigned> responseVersion {};
        unsigned requestVersion {};
        std::array<int, 3> firstPartition {}, lastPartition {};
        std::atomic<int> state {}; // 0 free, 1 worker owned, 2 audio owned
    };
    struct Lane {
        std::vector<Complex> history;
        std::vector<double> previous, input, jobInput, delayed;
        std::array<std::vector<double>, 3> output, nextOutput;
    };
    void buildBank(int index, const std::array<double, 3>&, unsigned version);
    void workerLoop();
    void startJob(int laneMask) noexcept;
    void advanceJob(int operations) noexcept;
    void finishJob() noexcept;
    enum class JobPhase { idle, input, forward, history, clear, multiply, inverse, output, inactive };
    JobPhase jobPhase {JobPhase::idle};
    Radix2FFT::Cursor fftCursor;
    int jobLane {}, jobFilter {}, jobPart {}, jobIndex {}, jobMask {}, jobBudget {};
    bool renderingOld {}, outputReady {};
    int fadePlaybackRemaining {};
    std::array<Bank, 4> banks;
    std::array<Lane, 4> lanes;
    Radix2FFT fft;
    std::vector<Complex> work;
    double sampleRate {48000};
    int taps {}, delay {}, hop {}, fftSize {}, bins {}, partitions {};
    int position {}, historyHead {}, delayPosition {}, quietRemaining {};
    int active {}, fadingFrom {-1}, fadePosition {}, fadeLength {};
    std::atomic<int> publishedActive {}, ready {-1};
    std::atomic<bool> stopping {true}, publishedTransition {};
    std::atomic<unsigned> schedulingOverruns {};
    std::array<std::atomic<double>, 3> requested {};
    std::atomic<unsigned> requestVersion {};
    std::array<double, 3> lastRequested {};
    std::thread worker;
};
}
