#include "LinearPhaseCrossover.h"
#include "Db.h"
#include "CrossoverNetwork.h"
#include <chrono>
namespace pontedsp::mc2000::dsp {
LinearPhaseCrossover::~LinearPhaseCrossover() { stopWorker(); }
void LinearPhaseCrossover::stopWorker() {
    stopping.store(true, std::memory_order_release);
    if (worker.joinable()) worker.join();
}
int LinearPhaseCrossover::profileTaps(double rate) noexcept {
    return 2 * static_cast<int>(std::ceil(rate * .256)) + 1;
}
std::vector<double> LinearPhaseCrossover::designLowPass(double rate, double frequency, int length) {
    const auto fc = clampFinite(frequency, 20.0, std::max(20.0, .45 * rate), 100.0) / rate;
    const int d = (length - 1) / 2;
    const int radius = std::min(d, static_cast<int>(std::ceil(rate * .256 * std::min(1.0, 20.0 / (fc * rate)))));
    std::vector<double> h(static_cast<std::size_t>(length));
    double sum = 0;
    for (int i = d - radius; i <= d; ++i) {
        const int offset = i - d;
        const double sinc = offset == 0 ? 2 * fc : std::sin(2 * std::numbers::pi * fc * offset) / (std::numbers::pi * offset);
        const double angle = std::numbers::pi * (i - d + radius) / radius;
        const double window = .42 - .5 * std::cos(angle) + .08 * std::cos(2 * angle);
        h[static_cast<std::size_t>(i)] = h[static_cast<std::size_t>(length - 1 - i)] = sinc * window;
    }
    for (double v : h) sum += v;
    for (double& v : h) v /= sum;
    return h;
}
double LinearPhaseCrossover::realResponse(const std::vector<double>& h, double rate, double frequency) noexcept {
    const int d = static_cast<int>(h.size() / 2);
    const double step = 2 * std::numbers::pi * frequency / rate;
    double result = h[static_cast<std::size_t>(d)];
    for (int i = 1; i <= d; ++i) result += 2 * h[static_cast<std::size_t>(d + i)] * std::cos(step * i);
    return result;
}
void LinearPhaseCrossover::prepare(double rate, const std::array<double, 3>& frequencies) {
    stopping.store(true, std::memory_order_release);
    if (worker.joinable()) worker.join();
    sampleRate = rate;
    taps = profileTaps(rate); delay = (taps - 1) / 2;
    hop = 2048;
    while (hop < rate / 48000.0 * 2048) hop *= 2;
    fftSize = 2 * hop; bins = fftSize;
    partitions = (taps + hop - 1) / hop;
    fft.prepare(fftSize);
    work.resize(static_cast<std::size_t>(fftSize));
    for (auto& bank : banks) {
        bank.state.store(0);
        for (auto& s : bank.spectra) s.resize(static_cast<std::size_t>(partitions * bins));
    }
    for (auto& lane : lanes) {
        lane.history.resize(static_cast<std::size_t>(partitions * bins));
        lane.input.resize(static_cast<std::size_t>(hop));
        lane.jobInput.resize(static_cast<std::size_t>(hop));
        lane.previous.resize(static_cast<std::size_t>(hop));
        lane.delayed.resize(static_cast<std::size_t>(latencySamples() + 1));
        for (auto& o : lane.output) o.resize(static_cast<std::size_t>(hop));
        for (auto& o : lane.nextOutput) o.resize(static_cast<std::size_t>(hop));
    }
    ready.store(-1); requestVersion.store(0); lastRequested = {};
    requestFrequencies(frequencies);
    active = 0; banks[0].state.store(2);
    buildBank(0, lastRequested, requestVersion.load());
    publishedActive.store(0, std::memory_order_release);
    reset();
    stopping.store(false, std::memory_order_release);
    worker = std::thread([this] { workerLoop(); });
}
void LinearPhaseCrossover::reset() noexcept {
    position = historyHead = delayPosition = quietRemaining = 0;
    jobPhase = JobPhase::idle; outputReady = false; fadePlaybackRemaining = 0; schedulingOverruns.store(0, std::memory_order_relaxed);
    if (fadingFrom >= 0) banks[static_cast<std::size_t>(fadingFrom)].state.store(0, std::memory_order_release);
    fadingFrom = -1; fadePosition = 0; publishedTransition.store(false, std::memory_order_relaxed);
    for (auto& lane : lanes) {
        std::fill(lane.history.begin(), lane.history.end(), Complex {});
        for (auto* v : {&lane.input, &lane.previous, &lane.jobInput, &lane.delayed}) std::fill(v->begin(), v->end(), 0.0);
        for (auto& o : lane.output) std::fill(o.begin(), o.end(), 0.0);
        for (auto& o : lane.nextOutput) std::fill(o.begin(), o.end(), 0.0);
    }
}
void LinearPhaseCrossover::requestFrequencies(const std::array<double, 3>& rawFrequencies) noexcept {
    const auto frequencies = CrossoverNetwork::effectiveFrequencies(rawFrequencies, sampleRate);
    if (frequencies == lastRequested) return;
    lastRequested = frequencies;
    requestVersion.fetch_add(1, std::memory_order_seq_cst);
    for (std::size_t i = 0; i < 3; ++i) requested[i].store(frequencies[i], std::memory_order_seq_cst);
    requestVersion.fetch_add(1, std::memory_order_seq_cst);
}
void LinearPhaseCrossover::buildBank(int index, const std::array<double, 3>& frequencies, unsigned version) {
    auto& bank = banks[static_cast<std::size_t>(index)];
    bank.responseVersion.fetch_add(1, std::memory_order_seq_cst);
    // Worker scratch is deliberately separate from the callback's work buffer.
    std::vector<Complex> scratch(static_cast<std::size_t>(fftSize));
    int responseSize = 1;
    while (responseSize < taps * 4) responseSize *= 2;
    Radix2FFT responseFFT; responseFFT.prepare(responseSize);
    std::vector<Complex> responseWork(static_cast<std::size_t>(responseSize));
    std::vector<double> amplitude(static_cast<std::size_t>(responseSize / 2 + 1));
    for (int filter = 0; filter < 3; ++filter) {
        const auto h = designLowPass(sampleRate, frequencies[static_cast<std::size_t>(filter)], taps);
        bank.frequencies[static_cast<std::size_t>(filter)].store(frequencies[static_cast<std::size_t>(filter)], std::memory_order_seq_cst);
        const int radius = std::min(delay, static_cast<int>(std::ceil(sampleRate * .256 * std::min(1.0, 20.0 / clampFinite(frequencies[static_cast<std::size_t>(filter)],20.0,.45 * sampleRate,100.0)))));
        bank.firstPartition[static_cast<std::size_t>(filter)] = (delay - radius) / hop;
        bank.lastPartition[static_cast<std::size_t>(filter)] = (delay + radius) / hop;
        auto& spectrum = bank.spectra[static_cast<std::size_t>(filter)];
        for (int part = 0; part < partitions; ++part) {
            std::fill(scratch.begin(), scratch.end(), Complex {});
            for (int i = 0; i < hop && part * hop + i < taps; ++i) scratch[static_cast<std::size_t>(i)] = h[static_cast<std::size_t>(part * hop + i)];
            fft.transform(scratch.data(), false);
            std::copy_n(scratch.data(), bins, spectrum.data() + part * bins);
        }
        std::fill(responseWork.begin(), responseWork.end(), Complex {});
        std::copy(h.begin(), h.end(), responseWork.begin());
        responseFFT.transform(responseWork.data(), false);
        for (int bin = 0; bin <= responseSize / 2; ++bin)
            amplitude[static_cast<std::size_t>(bin)] = (responseWork[static_cast<std::size_t>(bin)]
                * std::polar(1.0, 2 * std::numbers::pi * bin * delay / responseSize)).real();
        for (int point = 0; point < responsePoints; ++point) {
            const double frequency = point < curvePoints ? 20 * std::pow(std::min(20000.0, sampleRate * .5) / 20, double(point) / (curvePoints - 1))
                : double(point - curvePoints) * sampleRate / 2048;
            const double bin = std::clamp(frequency * responseSize / sampleRate, 0.0, double(responseSize / 2));
            const int lo = static_cast<int>(bin);
            const double mix = bin - lo;
            const auto at = [&](int i) {
                // Real symmetric FIR amplitude is even at DC and Nyquist.
                if (i < 0) i = -i;
                if (i > responseSize / 2) i = responseSize - i;
                return amplitude[static_cast<std::size_t>(i)];
            };
            const double a = at(lo - 1), b = at(lo), c = at(lo + 1), d = at(lo + 2);
            const double value = b + .5 * mix * (c - a + mix * (2 * a - 5 * b + 4 * c - d + mix * (3 * (b - c) + d - a)));
            bank.responses[static_cast<std::size_t>(filter)][static_cast<std::size_t>(point)].store(value, std::memory_order_seq_cst);
        }
    }
    bank.requestVersion = version;
    bank.responseVersion.fetch_add(1, std::memory_order_seq_cst);
}
void LinearPhaseCrossover::workerLoop() {
    unsigned built = banks[0].requestVersion;
    while (!stopping.load(std::memory_order_acquire)) {
        const unsigned version = requestVersion.load(std::memory_order_seq_cst);
        if (!(version & 1u) && version != built && ready.load(std::memory_order_acquire) < 0) {
            std::array<double, 3> f;
            for (std::size_t i = 0; i < 3; ++i) f[i] = requested[i].load(std::memory_order_seq_cst);
            if (requestVersion.load(std::memory_order_seq_cst) != version) continue;
            for (int i = 0; i < 4; ++i) {
                int free = 0;
                if (!banks[static_cast<std::size_t>(i)].state.compare_exchange_strong(free, 1, std::memory_order_acq_rel)) continue;
                buildBank(i, f, version);
                if (requestVersion.load(std::memory_order_seq_cst) == version) {
                    banks[static_cast<std::size_t>(i)].state.store(2, std::memory_order_release);
                    ready.store(i, std::memory_order_release); built = version;
                } else banks[static_cast<std::size_t>(i)].state.store(0, std::memory_order_release);
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
void LinearPhaseCrossover::startJob(int laneMask) noexcept {
    if (fadingFrom < 0) {
        const int next = ready.exchange(-1, std::memory_order_acq_rel);
        if (next >= 0) {
            if (banks[static_cast<std::size_t>(next)].requestVersion == requestVersion.load(std::memory_order_seq_cst)) {
                fadingFrom = active; active = next; fadePosition = 0;
                fadeLength = std::max(1, static_cast<int>(sampleRate * .02));
                publishedTransition.store(true, std::memory_order_relaxed);
                publishedActive.store(active, std::memory_order_release);
            } else banks[static_cast<std::size_t>(next)].state.store(0, std::memory_order_release);
        }
    }
    int stages = 0;
    for (int size = fftSize; size > 1; size /= 2) ++stages;
    const int forwardOperations = fftSize + fftSize / 2 * stages;
    const int inverseOperations = forwardOperations + fftSize;
    int total = 0;
    for (int lane = 0; lane < 4; lane += 2) {
        if (!(laneMask & (3 << lane))) { total += fftSize + 6 * hop; continue; }
        total += 2 * fftSize + 2 * forwardOperations;
        for (int filter = 0; filter < 3; ++filter) {
            const auto f = static_cast<std::size_t>(filter);
            const auto& bank = banks[static_cast<std::size_t>(active)];
            total += fftSize * (1 + bank.lastPartition[f] - bank.firstPartition[f] + 1) + 2 * inverseOperations + hop;
            if (fadingFrom >= 0) {
                const auto& old = banks[static_cast<std::size_t>(fadingFrom)];
                total += fftSize * (1 + old.lastPartition[f] - old.firstPartition[f] + 1) + 2 * inverseOperations + hop;
            }
        }
    }
    jobMask = laneMask; jobBudget = total / hop + 1;
    jobLane = jobFilter = jobPart = jobIndex = 0; renderingOld = false;
    jobPhase = (jobMask & 3) ? JobPhase::input : JobPhase::inactive;
}
void LinearPhaseCrossover::finishJob() noexcept {
    jobPhase = JobPhase::idle; outputReady = true;
    historyHead = (historyHead + 1) % partitions;
    if (fadingFrom >= 0 && (fadePosition += hop) >= fadeLength) {
        banks[static_cast<std::size_t>(fadingFrom)].state.store(0, std::memory_order_release);
        fadingFrom = -1; fadePlaybackRemaining = 2 * hop;
    }
}
void LinearPhaseCrossover::advanceJob(int operations) noexcept {
    while (operations > 0 && jobPhase != JobPhase::idle) {
        auto& lane = lanes[static_cast<std::size_t>(jobLane)];
        auto& right = lanes[static_cast<std::size_t>(jobLane + 1)];
        if (jobPhase == JobPhase::input) {
            const int count = std::min(operations, fftSize - jobIndex);
            for (int i = jobIndex; i < jobIndex + count; ++i) {
                const auto at = static_cast<std::size_t>(i < hop ? i : i - hop);
                work[static_cast<std::size_t>(i)] = i < hop ? Complex {lane.previous[at], right.previous[at]} : Complex {lane.jobInput[at], right.jobInput[at]};
            }
            jobIndex += count; operations -= count;
            if (jobIndex == fftSize) {jobIndex = 0; fftCursor = {}; jobPhase = JobPhase::forward;}
        } else if (jobPhase == JobPhase::forward || jobPhase == JobPhase::inverse) {
            // FFT butterflies have less sequential memory access than bin products.
            operations -= 2 * fft.advance(work.data(), fftCursor, std::max(1, operations / 2));
            if (fftCursor.phase == 3) {jobIndex = 0; jobPhase = jobPhase == JobPhase::forward ? JobPhase::history : JobPhase::output;}
        } else if (jobPhase == JobPhase::history) {
            const int count = std::min(operations, fftSize - jobIndex);
            std::copy_n(work.data() + jobIndex, count, lane.history.data() + historyHead * bins + jobIndex);
            jobIndex += count; operations -= count;
            if (jobIndex == fftSize) {jobIndex = 0; jobFilter = 0; renderingOld = false; jobPhase = JobPhase::clear;}
        } else if (jobPhase == JobPhase::clear) {
            const int count = std::min(operations, fftSize - jobIndex);
            std::fill_n(work.data() + jobIndex, count, Complex {});
            jobIndex += count; operations -= count;
            if (jobIndex == fftSize) {
                jobIndex = 0; jobPart = banks[static_cast<std::size_t>(renderingOld ? fadingFrom : active)].firstPartition[static_cast<std::size_t>(jobFilter)]; jobPhase = JobPhase::multiply;
            }
        } else if (jobPhase == JobPhase::multiply) {
            const auto& bank = banks[static_cast<std::size_t>(renderingOld ? fadingFrom : active)];
            const int slot = (historyHead + partitions - jobPart) % partitions;
            const auto* x = lane.history.data() + slot * bins;
            const auto* h = bank.spectra[static_cast<std::size_t>(jobFilter)].data() + jobPart * bins;
            const int count = std::min(operations, fftSize - jobIndex);
            for (int i = jobIndex; i < jobIndex + count; ++i) complexMultiplyAccumulate(work[static_cast<std::size_t>(i)],x[i],h[i]);
            jobIndex += count; operations -= count;
            if (jobIndex == fftSize) {
                jobIndex = 0;
                if (++jobPart > bank.lastPartition[static_cast<std::size_t>(jobFilter)]) {fftCursor = {}; fftCursor.inverse = true; jobPhase = JobPhase::inverse;}
            }
        } else if (jobPhase == JobPhase::output) {
            const int count = std::min(operations, hop - jobIndex);
            auto& out = lane.nextOutput[static_cast<std::size_t>(jobFilter)];
            auto& outRight = right.nextOutput[static_cast<std::size_t>(jobFilter)];
            for (int i = jobIndex; i < jobIndex + count; ++i) {
                const auto value = work[static_cast<std::size_t>(hop + i)];
                if (!renderingOld) {out[static_cast<std::size_t>(i)] = value.real(); outRight[static_cast<std::size_t>(i)] = value.imag();}
                else {
                    const double mix = std::min(1.0, double(fadePosition + i + 1) / fadeLength);
                    out[static_cast<std::size_t>(i)] = value.real() * (1 - mix) + out[static_cast<std::size_t>(i)] * mix;
                    outRight[static_cast<std::size_t>(i)] = value.imag() * (1 - mix) + outRight[static_cast<std::size_t>(i)] * mix;
                }
            }
            jobIndex += count; operations -= count;
            if (jobIndex == hop) {
                jobIndex = 0;
                if (fadingFrom >= 0 && !renderingOld) {renderingOld = true; jobPhase = JobPhase::clear;}
                else if (++jobFilter < 3) {renderingOld = false; jobPhase = JobPhase::clear;}
                else {
                    jobLane += 2; jobFilter = 0; renderingOld = false;
                    if (jobLane == 4) finishJob();
                    else jobPhase = (jobMask & (3 << jobLane)) ? JobPhase::input : JobPhase::inactive;
                }
            }
        } else { // No connected signals for this packed channel pair.
            const int count = std::min(operations, fftSize + 6 * hop - jobIndex);
            for (int i = jobIndex; i < jobIndex + count; ++i) {
                if (i < fftSize) lane.history[static_cast<std::size_t>(historyHead * bins + i)] = {};
                else {
                    const int at = i - fftSize;
                    auto& destination = at < 3 * hop ? lane : right;
                    destination.nextOutput[static_cast<std::size_t>((at / hop) % 3)][static_cast<std::size_t>(at % hop)] = 0;
                }
            }
            jobIndex += count; operations -= count;
            if (jobIndex == fftSize + 6 * hop) {
                jobIndex = 0; jobLane += 2;
                if (jobLane == 4) finishJob();
                else jobPhase = (jobMask & (3 << jobLane)) ? JobPhase::input : JobPhase::inactive;
            }
        }
    }
}

void LinearPhaseCrossover::processFrame(const std::array<double, 4>& input, int laneMask, int bands,
                                       std::array<std::array<double, 4>, 4>& output) noexcept {
    advanceJob(jobBudget);
    bool nonzero = false;
    for (int ch = 0; ch < 4; ++ch) {
        auto& lane = lanes[static_cast<std::size_t>(ch)];
        const double value = input[static_cast<std::size_t>(ch)];
        nonzero = nonzero || value != 0;
        lane.delayed[static_cast<std::size_t>(delayPosition)] = value;
        const double delayed = lane.delayed[static_cast<std::size_t>((delayPosition + 1) % (latencySamples() + 1))];
        lane.input[static_cast<std::size_t>(position)] = value;
        output[static_cast<std::size_t>(ch)].fill(0);
        double previous = 0;
        for (int band = 0; band < bands - 1; ++band) {
            const double cumulative = lane.output[static_cast<std::size_t>(band)][static_cast<std::size_t>(position)];
            output[static_cast<std::size_t>(ch)][static_cast<std::size_t>(band)] = cumulative - previous;
            previous = cumulative;
        }
        output[static_cast<std::size_t>(ch)][static_cast<std::size_t>(bands - 1)] = delayed - previous;
    }
    quietRemaining = nonzero ? taps + 4 * hop : std::max(0, quietRemaining - 1);
    delayPosition = (delayPosition + 1) % (latencySamples() + 1);
    if (fadePlaybackRemaining > 0 && --fadePlaybackRemaining == 0 && fadingFrom < 0) publishedTransition.store(false, std::memory_order_relaxed);
    if (++position == hop) {
        // A complete operation budget guarantees this branch never needs the
        // fallback. Preserve audio safely if a future edit breaks that budget.
        if (jobPhase != JobPhase::idle) {schedulingOverruns.fetch_add(1, std::memory_order_relaxed); advanceJob(100000000);}
        if (outputReady) {
            for (auto& lane : lanes) for (std::size_t filter = 0; filter < 3; ++filter) lane.output[filter].swap(lane.nextOutput[filter]);
            outputReady = false;
        }
        for (auto& lane : lanes) {lane.previous.swap(lane.jobInput); lane.jobInput.swap(lane.input);}
        startJob(laneMask); position = 0;
    }
}
bool LinearPhaseCrossover::isQuiet() const noexcept {
    return quietRemaining == 0 && fadingFrom < 0 && fadePlaybackRemaining == 0 && ready.load(std::memory_order_acquire) < 0
        && banks[static_cast<std::size_t>(active)].requestVersion == requestVersion.load(std::memory_order_seq_cst);
}
bool LinearPhaseCrossover::copyResponse(Responses& result, std::array<double, 3>& f, unsigned& version) const noexcept {
    const int index = publishedActive.load(std::memory_order_acquire);
    const auto& bank = banks[static_cast<std::size_t>(index)];
    const unsigned before = bank.responseVersion.load(std::memory_order_seq_cst);
    if (before == 0 || (before & 1u)) return false;
    for (std::size_t i = 0; i < 3; ++i) {
        f[i] = bank.frequencies[i].load(std::memory_order_seq_cst);
        for (std::size_t j = 0; j < responsePoints; ++j) result[i][j] = bank.responses[i][j].load(std::memory_order_seq_cst);
    }
    if (bank.responseVersion.load(std::memory_order_seq_cst) != before || publishedActive.load(std::memory_order_acquire) != index) return false;
    version = before * 4 + static_cast<unsigned>(index);
    return true;
}
}
