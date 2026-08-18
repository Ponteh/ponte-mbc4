#include <juce_audio_formats/juce_audio_formats.h>
#include "DSP/MultiBandCompressor.h"
#include "DSP/GainComputer.h"
#include "DSP/LinkwitzRiley4.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr double floorDb = -160.0;

struct AudioFile
{
    juce::AudioBuffer<float> samples;
    double sampleRate {};
};

AudioFile readAudio(const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr)
        throw std::runtime_error("Cannot read " + file.getFullPathName().toStdString());

    AudioFile result;
    result.sampleRate = reader->sampleRate;
    result.samples.setSize(static_cast<int>(reader->numChannels),
                           static_cast<int>(reader->lengthInSamples));
    if (!reader->read(&result.samples, 0, result.samples.getNumSamples(), 0, true, true))
        throw std::runtime_error("Read failed for " + file.getFullPathName().toStdString());
    return result;
}

juce::File findOne(const juce::File& directory, const std::vector<std::string>& tokens)
{
    for (const auto& file : directory.findChildFiles(juce::File::findFiles, false, "*.wav"))
    {
        const auto name = file.getFileName().toStdString();
        auto matches = true;
        for (const auto& token : tokens)
            matches = matches && name.find(token) != std::string::npos;
        if (matches) return file;
    }
    std::string description;
    for (const auto& token : tokens) description += " [" + token + "]";
    throw std::runtime_error("No WAV matches" + description);
}

double rms(const AudioFile& audio, const double startSeconds, const double endSeconds,
           const int channel = 0)
{
    const auto start = std::clamp(static_cast<int>(std::llround(startSeconds * audio.sampleRate)),
                                  0, audio.samples.getNumSamples());
    const auto end = std::clamp(static_cast<int>(std::llround(endSeconds * audio.sampleRate)),
                                start, audio.samples.getNumSamples());
    long double sum = 0.0;
    const auto* values = audio.samples.getReadPointer(
        std::clamp(channel, 0, audio.samples.getNumChannels() - 1));
    for (int sample = start; sample < end; ++sample)
        sum += static_cast<long double>(values[sample]) * values[sample];
    return end > start ? std::sqrt(static_cast<double>(sum / (end - start))) : 0.0;
}

double toDb(const double linear)
{
    return linear > 1.0e-16 ? 20.0 * std::log10(linear) : floorDb;
}

double gainDb(const AudioFile& input, const AudioFile& output,
              const double startSeconds, const double endSeconds, const int channel = 0)
{
    return toDb(rms(output, startSeconds, endSeconds, channel)
                / std::max(rms(input, startSeconds, endSeconds, channel), 1.0e-16));
}

double relativeDb(const AudioFile& numerator, const AudioFile& denominator,
                  const double startSeconds, const double endSeconds)
{
    return toDb(rms(numerator, startSeconds, endSeconds)
                / std::max(rms(denominator, startSeconds, endSeconds), 1.0e-16));
}

struct Segment { double start, end, value; };

void staticCurves(const juce::File& sourceDirectory, const juce::File& renderDirectory)
{
    const std::vector<Segment> negative {
        {0.0,2.0,-36},{2.4,4.4,-34},{4.8,6.8,-32},{7.2,9.2,-30},{9.6,11.6,-28},
        {12.0,14.0,-26},{14.4,16.4,-24},{16.8,18.8,-22},{19.2,21.2,-20},
        {21.6,23.6,-18},{24.0,26.0,-16},{26.4,28.4,-14},{28.8,30.8,-12}
    };
    const std::vector<Segment> positive {
        {0.0,2.0,-30},{2.4,4.4,-29},{4.8,6.8,-28},{7.2,9.2,-27},{9.6,11.6,-26},
        {12.0,14.0,-25},{14.4,16.4,-24},{16.8,18.8,-23},{19.2,21.2,-22},
        {21.6,23.6,-21},{24.0,26.0,-20},{26.4,28.4,-19},{28.8,30.8,-18},
        {31.2,33.2,-17},{33.6,35.6,-16},{36.0,38.0,-15},{38.4,40.4,-14},
        {40.8,42.8,-13},{43.2,45.2,-12},{45.6,47.6,-10},{48.0,50.0,-8},
        {50.4,52.4,-6},{52.8,54.8,-3}
    };

    const auto negZero = readAudio(findOne(renderDirectory, {"T06_", "knee0"}));
    const auto negMinus5 = readAudio(findOne(renderDirectory, {"T06_", "knee-5"}));
    const auto negMinus10 = readAudio(findOne(renderDirectory, {"T06_", "knee-10"}));
    std::cout << "\nNEGATIVE_KNEE input,K-5_delta,K-10_delta\n";
    for (const auto& segment : negative)
    {
        const auto a = std::max(segment.start, segment.end - 0.5);
        std::cout << segment.value << ',' << relativeDb(negMinus5, negZero, a, segment.end)
                  << ',' << relativeDb(negMinus10, negZero, a, segment.end) << '\n';
    }

    const auto posZero = readAudio(findOne(renderDirectory, {"T07_", "knee0"}));
    const auto plus5 = readAudio(findOne(renderDirectory, {"T07_", "knee+5."}));
    const auto plus10 = readAudio(findOne(renderDirectory, {"T07_", "knee+10."}));
    const auto plus125 = readAudio(findOne(renderDirectory, {"T07_", "knee+12.5"}));
    const auto plus15 = readAudio(findOne(renderDirectory, {"T07_", "knee+15"}));
    std::cout << "\nPOSITIVE_KNEE input,K+5_delta,K+10_delta,K+12.5_delta,K+15_delta\n";
    for (const auto& segment : positive)
    {
        const auto a = std::max(segment.start, segment.end - 0.5);
        std::cout << segment.value << ',' << relativeDb(plus5, posZero, a, segment.end)
                  << ',' << relativeDb(plus10, posZero, a, segment.end)
                  << ',' << relativeDb(plus125, posZero, a, segment.end)
                  << ',' << relativeDb(plus15, posZero, a, segment.end) << '\n';
    }

    const auto staticInput = readAudio(findOne(sourceDirectory, {"T05_"}));
    const auto staticOutput = readAudio(findOne(renderDirectory, {"T05_"}));
    const std::vector<double> levels {-48,-42,-36,-30,-27,-24,-21,-18,-15,-12,-9,-6,-3};
    std::cout << "\nHARD_KNEE input,gain_db,measured_output_db\n";
    for (std::size_t index = 0; index < levels.size(); ++index)
    {
        const auto end = 2.0 + 2.5 * static_cast<double>(index);
        const auto measuredGain = gainDb(staticInput, staticOutput, end - 0.5, end);
        std::cout << levels[index] << ',' << measuredGain << ',' << levels[index] + measuredGain << '\n';
    }
}

double measureFractionTime(const AudioFile& input, const AudioFile& output,
                           const double eventStart, const double searchEnd,
                           const double baselineGain, const double finalGain,
                           const double fraction, const bool attacking)
{
    constexpr double window = 0.01;
    constexpr double step = 0.001;
    const auto totalChange = finalGain - baselineGain;
    for (auto time = eventStart; time + window <= searchEnd; time += step)
    {
        const auto current = gainDb(input, output, time, time + window);
        const auto progress = totalChange != 0.0 ? (current - baselineGain) / totalChange : 0.0;
        if (progress >= fraction)
            return time - eventStart + (attacking ? window * 0.5 : 0.0);
    }
    return -1.0;
}

void ballistics(const juce::File& sourceDirectory, const juce::File& renderDirectory)
{
    const auto attackInput = readAudio(findOne(sourceDirectory, {"T08_"}));
    std::cout << "\nATTACK parameter_ms,t50_ms,t63_ms,t90_ms,final_gr_db\n";
    for (const auto& token : std::vector<std::pair<std::string,double>> {
        {"attack0.25",0.25},{"attack1msec",1.0},{"attack10msec",10.0},
        {"attack50msec",50.0},{"attack100msec",100.0}})
    {
        const auto output = readAudio(findOne(renderDirectory, {"T08_", token.first}));
        const auto baseline = gainDb(attackInput, output, 1.5, 1.95);
        const auto final = gainDb(attackInput, output, 3.5, 3.95);
        std::cout << token.second << ','
                  << 1000.0 * measureFractionTime(attackInput, output, 2.0, 3.9, baseline, final, 0.5, true) << ','
                  << 1000.0 * measureFractionTime(attackInput, output, 2.0, 3.9, baseline, final, 0.6321205588, true) << ','
                  << 1000.0 * measureFractionTime(attackInput, output, 2.0, 3.9, baseline, final, 0.9, true) << ','
                  << baseline - final << '\n';
    }

    const auto releaseInput = readAudio(findOne(sourceDirectory, {"T09_"}));
    std::cout << "\nRELEASE parameter_ms,t50_ms,t10percent_ms,start_gr_db\n";
    for (const auto& token : std::vector<std::pair<std::string,double>> {
        {"rel50msec",50.0},{"rel100msec",100.0},{"rel250msec",250.0},
        {"rel500msec",500.0},{"rel1000msec",1000.0}})
    {
        const auto output = readAudio(findOne(renderDirectory, {"T09_", token.first}));
        const auto baseline = gainDb(releaseInput, output, 1.5, 1.95);
        const auto compressed = gainDb(releaseInput, output, 3.5, 3.95);
        const auto t50 = measureFractionTime(releaseInput, output, 4.0, 6.0,
                                             compressed, baseline, 0.5, false);
        const auto t90 = measureFractionTime(releaseInput, output, 4.0, 6.0,
                                             compressed, baseline, 0.9, false);
        std::cout << token.second << ',' << 1000.0 * t50 << ',' << 1000.0 * t90
                  << ',' << baseline - compressed << '\n';
    }
}

void bite(const juce::File& renderDirectory)
{
    const auto minimum = readAudio(findOne(renderDirectory, {"T13_", "bitemin"}));
    const auto maximum = readAudio(findOne(renderDirectory, {"T13_", "bitemax"}));
    const std::vector<Segment> events {{0.5,0.81,0.0},{1.31,1.63,10.0},{2.13,2.54,100.0}};
    std::cout << "\nBITE_RISE rise_ms,max_relief_db,relief_at_100ms_db,steady_db\n";
    for (const auto& event : events)
    {
        auto peak = -160.0;
        for (auto time = event.start; time < event.end - 0.005; time += 0.001)
            peak = std::max(peak, relativeDb(maximum, minimum, time, time + 0.005));
        std::cout << event.value << ',' << peak << ','
                  << relativeDb(maximum, minimum, event.start + 0.1, event.start + 0.11) << ','
                  << relativeDb(maximum, minimum, event.end - 0.06, event.end - 0.01) << '\n';
    }

    std::cout << "\nBITE_FREQUENCY frequency_hz,max_relief_db,end_relief_db\n";
    const auto frequencyMin = readAudio(findOne(renderDirectory, {"T12_", "bitemin"}));
    const auto frequencyMax = readAudio(findOne(renderDirectory, {"T12_", "bitemax"}));
    for (const auto& event : std::vector<Segment> {
        {0.5,0.6,100},{1.0,1.1,500},{1.5,1.6,1000},{2.0,2.1,5000},{2.5,2.6,10000}})
    {
        auto peak = -160.0;
        for (auto time = event.start; time < event.end - 0.005; time += 0.001)
            peak = std::max(peak, relativeDb(frequencyMax, frequencyMin, time, time + 0.005));
        std::cout << event.value << ',' << peak << ','
                  << relativeDb(frequencyMax, frequencyMin, event.end - 0.01, event.end) << '\n';
    }
}

void typeComparison(const juce::File& sourceDirectory, const juce::File& renderDirectory)
{
    std::cout << "\nTYPE2_SECOND_EVENT level_dbfs,t2_minus_t1_at_start_db,t2_minus_t1_at_end_db\n";
    for (const auto level : {-20, -15, -10})
    {
        const auto prefix = "T10_Type1_vs_Type2_SecondEvent_" + std::to_string(level) + "dBFS";
        const auto one = readAudio(findOne(renderDirectory, {prefix, "t1"}));
        const auto two = readAudio(findOne(renderDirectory, {prefix, "t2"}));
        std::cout << level << ',' << relativeDb(two, one, 1.70, 1.72)
                  << ',' << relativeDb(two, one, 1.88, 1.90) << '\n';
    }
}

AudioFile renderModel(const AudioFile& source,
                      const pontedsp::mc2000::dsp::GlobalParameters& parameters)
{
    AudioFile output;
    output.sampleRate = source.sampleRate;
    output.samples.makeCopyOf(source.samples);
    pontedsp::mc2000::dsp::MultiBandCompressor engine;
    engine.setParameters(parameters);
    engine.prepare(source.sampleRate, 512, output.samples.getNumChannels());
    for (int start = 0; start < output.samples.getNumSamples(); start += 512)
    {
        const auto count = std::min(512, output.samples.getNumSamples() - start);
        std::array<float*, 2> channels {output.samples.getWritePointer(0, start), nullptr};
        if (output.samples.getNumChannels() > 1)
            channels[1] = output.samples.getWritePointer(1, start);
        engine.process(channels.data(), output.samples.getNumChannels(), count);
    }
    return output;
}

double normalisedErrorDb(const AudioFile& model, const AudioFile& reference,
                         const double startSeconds, const double endSeconds)
{
    const auto start = std::clamp(static_cast<int>(std::llround(startSeconds * model.sampleRate)),
                                  0, model.samples.getNumSamples());
    const auto end = std::clamp(static_cast<int>(std::llround(endSeconds * model.sampleRate)),
                                start, model.samples.getNumSamples());
    long double error = 0.0;
    long double signal = 0.0;
    const auto* a = model.samples.getReadPointer(0);
    const auto* b = reference.samples.getReadPointer(0);
    for (int sample = start; sample < end; ++sample)
    {
        const auto difference = static_cast<double>(a[sample]) - b[sample];
        error += difference * difference;
        signal += static_cast<double>(b[sample]) * b[sample];
    }
    return 10.0 * std::log10(static_cast<double>(error / std::max(signal, 1.0e-30L)));
}

void modelComparison(const juce::File& sourceDirectory, const juce::File& renderDirectory)
{
    using namespace pontedsp::mc2000::dsp;
    GlobalParameters parameters;
    parameters.numBands = 2;
    parameters.crossoverHz = {1000.0, 5000.0, 10000.0};
    parameters.bands[0].solo = true;
    parameters.bands[0].thresholdDb = -24.0;
    parameters.bands[0].ratio = 10.0;
    parameters.bands[0].knee = 0.0;
    parameters.bands[0].bite = 1.0;
    parameters.bands[0].releaseMs = 1000.0;
    parameters.bands[0].tcMode = TCMode::type1;

    const auto attackInput = readAudio(findOne(sourceDirectory, {"T08_"}));
    std::cout << "\nMODEL_ATTACK parameter_ms,nrmse_db,t63_ms\n";
    for (const auto& token : std::vector<std::pair<std::string,double>> {
        {"attack0.25",0.25},{"attack1msec",1.0},{"attack10msec",10.0},
        {"attack50msec",50.0},{"attack100msec",100.0}})
    {
        parameters.bands[0].attackMs = token.second;
        const auto model = renderModel(attackInput, parameters);
        const auto reference = readAudio(findOne(renderDirectory, {"T08_", token.first}));
        const auto baseline = gainDb(attackInput, model, 1.5, 1.95);
        const auto final = gainDb(attackInput, model, 3.5, 3.95);
        const auto t63 = measureFractionTime(attackInput, model, 2.0, 3.9,
                                             baseline, final, 0.6321205588, true);
        std::cout << token.second << ',' << normalisedErrorDb(model, reference, 2.0, 4.0)
                  << ',' << 1000.0 * t63 << '\n';
    }

    const auto biteInput = readAudio(findOne(sourceDirectory, {"T13_"}));
    {
        LinkwitzRiley4 crossover;
        crossover.prepare(biteInput.sampleRate, 20.0);
        Ballistics timing;
        timing.prepare(biteInput.sampleRate);
        BiteProcessor biteProbe;
        biteProbe.prepare(biteInput.sampleRate);
        GainComputer gainComputer;
        std::array<double, 3> maximumRelief {};
        std::array<double, 3> transientAtMaximum {};
        const std::array eventStarts {0.5, 1.31, 2.13};
        const std::array eventEnds {0.81, 1.63, 2.54};
        const auto* samples = biteInput.samples.getReadPointer(0);
        for (int n = 0; n < biteInput.samples.getNumSamples(); ++n)
        {
            const auto high = crossover.split(samples[n]).second;
            const auto detector = std::abs(high);
            const auto target = detector > 1.0e-12
                ? gainComputer.computeGainReductionDb(toDb(detector), -24.0, 10.0, 0.0) : 0.0;
            const auto normal = timing.process(target, detector, 10.0, 300.0, TCMode::type1);
            const auto modified = biteProbe.process(normal, detector, 50.0);
            const auto relief = normal - modified;
            const auto time = n / biteInput.sampleRate;
            for (std::size_t event = 0; event < eventStarts.size(); ++event)
                if (time >= eventStarts[event] && time < eventEnds[event]
                    && relief > maximumRelief[event])
                {
                    maximumRelief[event] = relief;
                    transientAtMaximum[event] = biteProbe.getLastTransientNormalised();
                }
        }
        std::cout << "\nBITE_CHAIN_PROBE rise_ms,max_sample_relief,transient_at_max\n";
        for (std::size_t event = 0; event < eventStarts.size(); ++event)
            std::cout << (event == 0 ? 0 : event == 1 ? 10 : 100) << ','
                      << maximumRelief[event] << ',' << transientAtMaximum[event] << '\n';
    }
    {
        auto fast = 0.0;
        auto slow = 0.0;
        const auto coefficient = [&](const double seconds)
        {
            return std::exp(-1.0 / (biteInput.sampleRate * seconds));
        };
        const auto fastAttack = coefficient(0.0005);
        const auto fastRelease = coefficient(0.012);
        const auto slowAttack = coefficient(0.018);
        const auto slowRelease = coefficient(0.080);
        std::array<double, 3> maximumDifference {};
        std::array<double, 3> maximumRatio {};
        const std::array eventStarts {0.5, 1.31, 2.13};
        const std::array eventEnds {0.81, 1.63, 2.54};
        const auto* samples = biteInput.samples.getReadPointer(0);
        for (int n = 0; n < biteInput.samples.getNumSamples(); ++n)
        {
            const auto detector = std::abs(static_cast<double>(samples[n]));
            const auto fastC = detector > fast ? fastAttack : fastRelease;
            const auto slowC = detector > slow ? slowAttack : slowRelease;
            fast = fastC * fast + (1.0 - fastC) * detector;
            slow = slowC * slow + (1.0 - slowC) * detector;
            const auto difference = std::max(0.0, fast - slow);
            const auto ratio = difference / std::max(fast, 1.0e-12);
            const auto time = n / biteInput.sampleRate;
            for (std::size_t event = 0; event < eventStarts.size(); ++event)
                if (time >= eventStarts[event] && time < eventEnds[event])
                {
                    maximumDifference[event] = std::max(maximumDifference[event], difference);
                    maximumRatio[event] = std::max(maximumRatio[event], ratio);
                }
        }
        std::cout << "\nBITE_ENVELOPE rise_ms,max_difference,max_ratio\n";
        for (std::size_t event = 0; event < eventStarts.size(); ++event)
            std::cout << (event == 0 ? 0 : event == 1 ? 10 : 100) << ','
                      << maximumDifference[event] << ',' << maximumRatio[event] << '\n';
    }
    parameters.crossoverHz[0] = 20.0;
    parameters.bands[0].solo = false;
    parameters.bands[0].ratio = 1.0;
    parameters.bands[1].solo = true;
    parameters.bands[1].thresholdDb = -24.0;
    parameters.bands[1].ratio = 10.0;
    parameters.bands[1].attackMs = 10.0;
    parameters.bands[1].releaseMs = 300.0;
    parameters.bands[1].tcMode = TCMode::type1;
    parameters.bands[1].bite = 1.0;
    const auto modelMin = renderModel(biteInput, parameters);
    parameters.bands[1].bite = 50.0;
    const auto modelMax = renderModel(biteInput, parameters);
    const std::vector<Segment> events {{0.5,0.81,0.0},{1.31,1.63,10.0},{2.13,2.54,100.0}};
    std::cout << "\nMODEL_BITE rise_ms,max_relief_db\n";
    for (const auto& event : events)
    {
        auto peak = -160.0;
        for (auto time = event.start; time < event.end - 0.005; time += 0.001)
            peak = std::max(peak, relativeDb(modelMax, modelMin, time, time + 0.005));
        std::cout << event.value << ',' << peak << '\n';
    }

    parameters.crossoverHz[0] = 1000.0;
    parameters.bands[0].solo = true;
    parameters.bands[0].thresholdDb = -24.0;
    parameters.bands[0].ratio = 10.0;
    parameters.bands[0].attackMs = 2.5;
    parameters.bands[0].releaseMs = 1000.0;
    parameters.bands[0].bite = 1.0;
    parameters.bands[1].solo = false;
    parameters.bands[1].ratio = 1.0;
    std::cout << "\nMODEL_TYPE2 level_dbfs,model_start,reference_start,model_end,reference_end\n";
    for (const auto level : {-20, -15, -10})
    {
        const auto prefix = "T10_Type1_vs_Type2_SecondEvent_" + std::to_string(level) + "dBFS";
        const auto source = readAudio(findOne(sourceDirectory, {prefix}));
        parameters.bands[0].tcMode = TCMode::type1;
        const auto modelOne = renderModel(source, parameters);
        parameters.bands[0].tcMode = TCMode::type2;
        const auto modelTwo = renderModel(source, parameters);
        const auto referenceOne = readAudio(findOne(renderDirectory, {prefix, "t1"}));
        const auto referenceTwo = readAudio(findOne(renderDirectory, {prefix, "t2"}));
        std::cout << level << ',' << relativeDb(modelTwo, modelOne, 1.70, 1.72)
                  << ',' << relativeDb(referenceTwo, referenceOne, 1.70, 1.72)
                  << ',' << relativeDb(modelTwo, modelOne, 1.88, 1.90)
                  << ',' << relativeDb(referenceTwo, referenceOne, 1.88, 1.90) << '\n';
    }

    std::cout << "\nMODEL_TYPE2_DELAY delay_ms,nrmse_db\n";
    for (const auto delay : {100, 250, 500, 750})
    {
        const auto prefix = "T11_Type2_Delay_" + std::to_string(delay) + "ms";
        const auto source = readAudio(findOne(sourceDirectory, {prefix}));
        const auto reference = readAudio(findOne(renderDirectory, {prefix}));
        parameters.bands[0].tcMode = TCMode::type2;
        const auto model = renderModel(source, parameters);
        std::cout << delay << ',' << normalisedErrorDb(model, reference, 1.0,
                                                       source.samples.getNumSamples() / source.sampleRate) << '\n';
    }
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: MC2000BlackBoxAnalysis <source-dir> <render-dir>\n";
            return 2;
        }
        const juce::File sourceDirectory(juce::String::fromUTF8(argv[1]));
        const juce::File renderDirectory(juce::String::fromUTF8(argv[2]));
        if (!sourceDirectory.isDirectory() || !renderDirectory.isDirectory())
            throw std::runtime_error("Both arguments must be directories");

        std::cout << std::fixed << std::setprecision(6);
        staticCurves(sourceDirectory, renderDirectory);
        ballistics(sourceDirectory, renderDirectory);
        typeComparison(sourceDirectory, renderDirectory);
        bite(renderDirectory);
        modelComparison(sourceDirectory, renderDirectory);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Black-box analysis failed: " << error.what() << '\n';
        return 1;
    }
}
