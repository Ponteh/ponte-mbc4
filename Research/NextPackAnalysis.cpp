#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double floorDb = -160.0;

struct AudioFile
{
    juce::AudioBuffer<float> samples;
    double sampleRate {};
};

struct Stats
{
    int count {};
    double minimum { std::numeric_limits<double>::infinity() };
    double maximum { -std::numeric_limits<double>::infinity() };
    double sum {};

    void add(const double value)
    {
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
        sum += value;
        ++count;
    }

    double mean() const { return count == 0 ? 0.0 : sum / count; }
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

std::vector<juce::File> wavFiles(const juce::File& directory)
{
    const auto juceFiles = directory.findChildFiles(juce::File::findFiles, false, "*.wav");
    std::vector<juce::File> files;
    files.reserve(static_cast<std::size_t>(juceFiles.size()));
    for (const auto& file : juceFiles)
        files.push_back(file);
    std::sort(files.begin(), files.end(), [] (const auto& a, const auto& b)
    {
        return a.getFileName() < b.getFileName();
    });
    return files;
}

double toDb(const double value)
{
    return value > 1.0e-16 ? 20.0 * std::log10(value) : floorDb;
}

double rms(const AudioFile& audio, const int start, const int end, const int channel = 0)
{
    const auto begin = std::clamp(start, 0, audio.samples.getNumSamples());
    const auto finish = std::clamp(end, begin, audio.samples.getNumSamples());
    const auto* values = audio.samples.getReadPointer(
        std::clamp(channel, 0, audio.samples.getNumChannels() - 1));
    long double sum = 0.0;
    for (auto index = begin; index < finish; ++index)
        sum += static_cast<long double>(values[index]) * values[index];
    return finish > begin ? std::sqrt(static_cast<double>(sum / (finish - begin))) : 0.0;
}

double errorDb(const AudioFile& first, const AudioFile& second)
{
    if (std::abs(first.sampleRate - second.sampleRate) > 1.0e-3
        || first.samples.getNumSamples() != second.samples.getNumSamples())
        throw std::runtime_error("Render format mismatch");

    long double error = 0.0;
    long double signal = 0.0;
    const auto channels = std::min(first.samples.getNumChannels(), second.samples.getNumChannels());
    for (auto channel = 0; channel < channels; ++channel)
    {
        const auto* a = first.samples.getReadPointer(channel);
        const auto* b = second.samples.getReadPointer(channel);
        for (auto sample = 0; sample < first.samples.getNumSamples(); ++sample)
        {
            const auto difference = static_cast<double>(a[sample]) - b[sample];
            error += difference * difference;
            signal += static_cast<double>(b[sample]) * b[sample];
        }
    }
    return 10.0 * std::log10(static_cast<double>(error / std::max(signal, 1.0e-30L)));
}

std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [] (const unsigned char c)
    {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string removeDuplicateSuffix(std::string name)
{
    const auto close = name.rfind(").wav");
    const auto open = close == std::string::npos ? std::string::npos : name.rfind('(', close);
    if (open != std::string::npos)
    {
        const auto number = name.substr(open + 1, close - open - 1);
        if (!number.empty() && std::all_of(number.begin(), number.end(), [] (const unsigned char c)
            { return std::isdigit(c) != 0; }))
            name.erase(open, close - open + 1);
    }
    return name;
}

struct RenderName { std::string prefix; std::string sourceName; };

bool splitRenderName(const juce::File& file, RenderName& result)
{
    const auto name = file.getFileName().toStdString();
    const auto separator = name.find(" - ");
    if (separator == std::string::npos) return false;
    result.prefix = name.substr(0, separator);
    result.sourceName = removeDuplicateSuffix(name.substr(separator + 3));
    return true;
}

juce::File sourceFor(const juce::File& directory, const RenderName& render)
{
    return directory.getChildFile(juce::String(render.sourceName));
}

double ratioAtSeconds(const AudioFile& numerator, const AudioFile& denominator,
                      const double seconds, const double windowSeconds = 0.004,
                      const int channel = 0)
{
    if (std::abs(numerator.sampleRate - denominator.sampleRate) > 1.0e-3)
        throw std::runtime_error("Pair sample-rate mismatch");
    const auto start = static_cast<int>(std::llround(seconds * numerator.sampleRate));
    const auto count = std::max(1, static_cast<int>(std::llround(windowSeconds * numerator.sampleRate)));
    return toDb(rms(numerator, start, start + count, channel)
                / std::max(rms(denominator, start, start + count, channel), 1.0e-16));
}

std::array<double, 5> eventRatios(const AudioFile& numerator, const AudioFile& denominator)
{
    constexpr std::array offsets { 0.005, 0.010, 0.025, 0.050, 0.095 };
    constexpr std::array events { 2.0, 5.0, 8.0 };
    std::array<double, 5> result {};
    for (auto index = 0U; index < offsets.size(); ++index)
    {
        for (const auto event : events)
            result[index] += ratioAtSeconds(numerator, denominator, event + offsets[index]);
        result[index] /= static_cast<double>(events.size());
    }
    return result;
}

void printStats(const std::string& label, const Stats& stats)
{
    std::cout << label << ',' << stats.count << ',' << stats.minimum << ','
              << stats.maximum << ',' << stats.mean() << '\n';
}

void analyseAuto(const juce::File& directory)
{
    std::map<std::string, Stats> minMaxErrors;
    std::map<std::string, Stats> gains;
    for (const auto& file : wavFiles(directory))
    {
        RenderName render;
        if (!splitRenderName(file, render)) continue;
        const auto source = sourceFor(directory, render);
        if (!source.existsAsFile())
            throw std::runtime_error("Missing Auto source " + render.sourceName);

        const auto audio = readAudio(file);
        const auto input = readAudio(source);
        if (render.sourceName.find("A02_Continuous") != std::string::npos)
            gains[render.prefix].add(ratioAtSeconds(audio, input, 6.0, 0.25));
        if (!render.prefix.ends_with("-min")) continue;
        const auto maximumName = render.prefix.substr(0, render.prefix.size() - 4) + "-max - "
                               + render.sourceName;
        const auto maximum = directory.getChildFile(juce::String(maximumName));
        if (!maximum.existsAsFile())
            throw std::runtime_error("Missing Auto max pair " + maximumName);
        minMaxErrors[render.prefix.substr(0, render.prefix.size() - 4)].add(
            errorDb(audio, readAudio(maximum)));
    }

    std::cout << "\nAUTO_MIN_MAX,model,pairs,min_error_db,max_error_db,mean_error_db\n";
    for (const auto& [model, stats] : minMaxErrors) printStats(model, stats);
    std::cout << "AUTO_STEADY_GAIN,prefix,pairs,min_gain_db,max_gain_db,mean_gain_db\n";
    for (const auto& [prefix, stats] : gains) printStats(prefix, stats);
}

void analyseBite(const juce::File& directory)
{
    struct BiteStats { int pairs {}; std::array<Stats, 5> relief; };
    std::map<std::string, BiteStats> groups;
    std::map<std::string, BiteStats> modes;
    for (const auto& file : wavFiles(directory))
    {
        RenderName render;
        if (!splitRenderName(file, render)) continue;
        const auto prefix = lower(render.prefix);
        if (prefix.find("bite5,type1") == std::string::npos
            && prefix.find("bite10,type1") == std::string::npos
            && prefix.find("bite5,type2") == std::string::npos
            && prefix.find("bite5,auto") == std::string::npos) continue;

        auto referencePrefix = render.prefix;
        std::string label;
        const auto isBiteFive = prefix.find("bite5,type1") != std::string::npos;
        const auto isBiteTen = prefix.find("bite10,type1") != std::string::npos;
        const auto sourceLower = lower(render.sourceName);
        const auto soloHasProgram = [&]
        {
            if (prefix.find("solo") == std::string::npos) return true;
            if (prefix.find("sololow") != std::string::npos) return sourceLower.find("_f100hz_") != std::string::npos;
            if (prefix.find("solohigh") != std::string::npos) return sourceLower.find("_f10000hz_") != std::string::npos;
            if (prefix.find("solomid2") != std::string::npos) return sourceLower.find("_f5000hz_") != std::string::npos;
            return sourceLower.find("_f1000hz_") != std::string::npos;
        };
        if (!soloHasProgram()) continue;
        if (isBiteFive)
        {
            referencePrefix.replace(referencePrefix.find("bite5"), 5, "bite1");
            label = "bite5_vs_bite1," + render.prefix.substr(render.prefix.find_last_of(',') + 1);
        }
        else if (isBiteTen)
        {
            referencePrefix.replace(referencePrefix.find("bite10"), 6, "bite1");
            label = "bite10_vs_bite1," + render.prefix.substr(render.prefix.find_last_of(',') + 1);
        }
        else
        {
            const auto typePosition = referencePrefix.find("type2");
            const auto autoPosition = referencePrefix.find("auto");
            if (typePosition != std::string::npos) referencePrefix.replace(typePosition, 5, "type1");
            if (autoPosition != std::string::npos) referencePrefix.replace(autoPosition, 4, "type1");
            label = typePosition != std::string::npos ? "type2_vs_type1,all" : "auto_vs_type1,all";
        }

        // The reference set uses the earlier labels "solomdi"/"solomdi2"
        // for two mid bands; later BITE passes use "solomid"/"solomid2".
        // They denote the same solo states under TEST_SPEC.md.
        const auto firstTypo = referencePrefix.find("solomid");
        if (firstTypo != std::string::npos)
            referencePrefix.replace(firstTypo, 7, "solomdi");
        const auto secondTypo = referencePrefix.find("solomid2");
        if (secondTypo != std::string::npos)
            referencePrefix.replace(secondTypo, 8, "solomdi2");

        const auto reference = directory.getChildFile(juce::String(referencePrefix + " - "
                                                                     + render.sourceName));
        if (!reference.existsAsFile())
            throw std::runtime_error("Missing BITE reference " + reference.getFileName().toStdString());
        const auto response = eventRatios(readAudio(file), readAudio(reference));
        auto& stats = isBiteFive || isBiteTen ? groups[label] : modes[label];
        ++stats.pairs;
        for (auto index = 0U; index < response.size(); ++index) stats.relief[index].add(response[index]);
    }

    const auto print = [] (const std::string& title, const auto& values)
    {
        std::cout << '\n' << title
                  << ",comparison,pairs,at_5ms_db,at_10ms_db,at_25ms_db,at_50ms_db,at_95ms_db\n";
        for (const auto& [label, stats] : values)
        {
            std::cout << label << ',' << stats.pairs;
            for (const auto& point : stats.relief) std::cout << ',' << point.mean();
            std::cout << '\n';
        }
    };
    print("BITE_TRANSIENT_RELIEF", groups);
    print("BITE_TIME_CONSTANT_DELTA", modes);
}

void analyseStereo(const juce::File& directory)
{
    std::map<std::string, Stats> difference;
    std::map<std::string, Stats> linkedGain;
    for (const auto& file : wavFiles(directory))
    {
        RenderName render;
        if (!splitRenderName(file, render)) continue;
        const auto source = sourceFor(directory, render);
        if (!source.existsAsFile()) throw std::runtime_error("Missing stereo source " + render.sourceName);
        const auto input = readAudio(source);
        const auto output = readAudio(file);
        const auto left = ratioAtSeconds(output, input, 8.0, 1.0, 0);
        const auto right = ratioAtSeconds(output, input, 8.0, 1.0, 1);
        difference[render.prefix].add(std::abs(left - right));
        linkedGain[render.prefix].add((left + right) * 0.5);
    }
    std::cout << "\nSTEREO_LINK,prefix,pairs,min_left_right_delta_db,max_left_right_delta_db,mean_left_right_delta_db\n";
    for (const auto& [prefix, stats] : difference) printStats(prefix, stats);
    std::cout << "STEREO_STEADY_GAIN,prefix,pairs,min_gain_db,max_gain_db,mean_gain_db\n";
    for (const auto& [prefix, stats] : linkedGain) printStats(prefix, stats);
}

void analyseSampleRates(const juce::File& directory)
{
    std::map<std::string, Stats> gains;
    for (const auto& file : wavFiles(directory))
    {
        RenderName render;
        if (!splitRenderName(file, render)) continue;
        const auto source = sourceFor(directory, render);
        if (!source.existsAsFile()) throw std::runtime_error("Missing sample-rate source " + render.sourceName);
        const auto input = readAudio(source);
        const auto output = readAudio(file);
        if (std::abs(input.sampleRate - output.sampleRate) > 1.0e-3)
        {
            std::cout << "SAMPLE_RATE_FORMAT_MISMATCH," << file.getFileName().toStdString()
                      << ',' << input.sampleRate << ',' << output.sampleRate << '\n';
            continue;
        }
        const auto impulse = render.sourceName.find("Impulse") != std::string::npos;
        gains[render.prefix + "," + (impulse ? "impulse" : "auto") + ","
              + std::to_string(static_cast<int>(input.sampleRate))]
            .add(ratioAtSeconds(output, input, impulse ? 1.0 : 10.0, 0.25));
    }
    std::cout << "\nSAMPLE_RATE_STEADY,prefix_and_kind_and_rate,pairs,min_gain_db,max_gain_db,mean_gain_db\n";
    for (const auto& [key, stats] : gains) printStats(key, stats);
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 2 || argc > 3)
        {
            std::cerr << "Usage: MC2000NextPackAnalysis <next-pack-root> [auto|bite|stereo|sample]\n";
            return 2;
        }
        const juce::File root(juce::String::fromUTF8(argv[1]));
        if (!root.isDirectory()) throw std::runtime_error("Argument must be the next validation pack root");
        const auto section = argc == 3 ? lower(argv[2]) : std::string("all");
        std::cout << std::fixed << std::setprecision(6);
        if (section == "all" || section == "auto") analyseAuto(root.getChildFile("01_AUTO_CROSSOVER"));
        if (section == "all" || section == "bite") analyseBite(root.getChildFile("02_BITE"));
        if (section == "all" || section == "stereo") analyseStereo(root.getChildFile("05_STEREO_DETECTOR"));
        if (section == "all" || section == "sample") analyseSampleRates(root.getChildFile("06_SAMPLE_RATE"));
        if (section != "all" && section != "auto" && section != "bite"
            && section != "stereo" && section != "sample")
            throw std::runtime_error("Unknown section: " + section);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Next-pack analysis failed: " << error.what() << '\n';
        return 1;
    }
}
