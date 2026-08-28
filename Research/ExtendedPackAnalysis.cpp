#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
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

struct GainRange
{
    int windows {};
    double minimum { std::numeric_limits<double>::infinity() };
    double maximum { -std::numeric_limits<double>::infinity() };
    double mean {};
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

GainRange gainRange(const AudioFile& input, const AudioFile& output, const int channel = 0)
{
    if (std::abs(input.sampleRate - output.sampleRate) > 1.0e-3)
        throw std::runtime_error("Input/render sample-rate mismatch");

    const auto window = std::max(1, static_cast<int>(std::llround(input.sampleRate * 0.010)));
    const auto length = std::min(input.samples.getNumSamples(), output.samples.getNumSamples());
    GainRange result;
    for (auto start = 0; start + window <= length; start += window)
    {
        const auto inputRms = rms(input, start, start + window, channel);
        if (inputRms < 1.0e-7)
            continue;
        const auto gain = toDb(rms(output, start, start + window, channel) / inputRms);
        result.minimum = std::min(result.minimum, gain);
        result.maximum = std::max(result.maximum, gain);
        result.mean += gain;
        ++result.windows;
    }
    if (result.windows > 0)
        result.mean /= result.windows;
    else
        result.minimum = result.maximum = result.mean = 0.0;
    return result;
}

double errorDb(const AudioFile& first, const AudioFile& second)
{
    if (std::abs(first.sampleRate - second.sampleRate) > 1.0e-3
        || first.samples.getNumSamples() != second.samples.getNumSamples())
        throw std::runtime_error("Render format mismatch");

    long double error = 0.0;
    long double signal = 0.0;
    for (auto channel = 0; channel < std::min(first.samples.getNumChannels(), second.samples.getNumChannels()); ++channel)
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

std::string withoutSuffix(const std::string& text, const std::string& suffix)
{
    if (text.size() < suffix.size() || !text.ends_with(suffix))
        return text;
    return text.substr(0, text.size() - suffix.size());
}

bool hasCsvToken(const std::string& text, const std::string& token)
{
    auto start = std::size_t {};
    while (start <= text.size())
    {
        const auto end = text.find(',', start);
        if (text.substr(start, end == std::string::npos ? end : end - start) == token)
            return true;
        if (end == std::string::npos)
            return false;
        start = end + 1;
    }
    return false;
}

void printRange(const std::string& id, const GainRange& range)
{
    std::cout << id << ',' << range.windows << ',' << range.minimum << ','
              << range.maximum << ',' << range.mean << '\n';
}

void analyseAuto(const juce::File& directory)
{
    std::cout << "\nAUTO_RENDER,id,min_max_error_db,min_gain_db,max_gain_db,mean_gain_db\n";
    for (const auto& minimumFile : wavFiles(directory))
    {
        const auto minimumName = minimumFile.getFileName().toStdString();
        if (!minimumName.ends_with("-min.wav"))
            continue;
        const auto stem = withoutSuffix(minimumName, "-min.wav");
        const auto source = directory.getChildFile(juce::String(stem) + ".wav");
        const auto maximum = directory.getChildFile(juce::String(stem) + "-max.wav");
        if (!source.existsAsFile() || !maximum.existsAsFile())
            throw std::runtime_error("Incomplete Auto triplet for " + stem);
        const auto input = readAudio(source);
        const auto minimum = readAudio(minimumFile);
        const auto maximumAudio = readAudio(maximum);
        const auto range = gainRange(input, minimum);
        std::cout << stem << ',' << errorDb(minimum, maximumAudio) << ','
                  << range.minimum << ',' << range.maximum << ',' << range.mean << '\n';
    }
}

void analyseProPairs(const juce::File& directory)
{
    std::map<std::string, std::vector<GainRange>> groups;
    auto complete = 0;
    for (const auto& renderFile : wavFiles(directory))
    {
        const auto name = renderFile.getFileName().toStdString();
        if (!name.ends_with("-pro.wav"))
            continue;
        const auto sourceName = withoutSuffix(name, "-pro.wav") + ".wav";
        const auto sourceFile = directory.getChildFile(juce::String(sourceName));
        if (!sourceFile.existsAsFile())
            throw std::runtime_error("Missing source for " + name);
        const auto source = readAudio(sourceFile);
        const auto render = readAudio(renderFile);
        auto category = std::string("other");
        if (name.find("_Attack_") != std::string::npos) category = "attack";
        else if (name.find("_Knee_") != std::string::npos) category = "knee";
        else if (name.find("_Release_") != std::string::npos) category = "release";
        else if (name.find("_Type2_") != std::string::npos) category = "type2";
        groups[category].push_back(gainRange(source, render));
        ++complete;
    }

    std::cout << "\nDYNAMICS_PAIRS,category,pairs,min_gain_db,max_gain_db,mean_gain_db\n";
    for (const auto& [category, ranges] : groups)
    {
        auto minimum = std::numeric_limits<double>::infinity();
        auto maximum = -std::numeric_limits<double>::infinity();
        auto mean = 0.0;
        for (const auto& range : ranges)
        {
            minimum = std::min(minimum, range.minimum);
            maximum = std::max(maximum, range.maximum);
            mean += range.mean;
        }
        std::cout << category << ',' << ranges.size() << ',' << minimum << ','
                  << maximum << ',' << mean / std::max<std::size_t>(1, ranges.size()) << '\n';
    }
    std::cout << "DYNAMICS_PAIRS_TOTAL," << complete << "\n";
}

double sineThdDb(const AudioFile& audio, const double frequency)
{
    const auto start = std::min(audio.samples.getNumSamples() / 4,
                                static_cast<int>(audio.sampleRate * 0.5));
    const auto count = std::min(static_cast<int>(audio.sampleRate), audio.samples.getNumSamples() - start);
    if (count < 64 || frequency <= 0.0 || frequency * 2.0 >= audio.sampleRate * 0.5)
        return floorDb;

    const auto magnitude = [&](const double target)
    {
        auto real = 0.0;
        auto imaginary = 0.0;
        const auto* values = audio.samples.getReadPointer(0);
        for (auto index = 0; index < count; ++index)
        {
            const auto phase = 2.0 * juce::MathConstants<double>::pi * target * index / audio.sampleRate;
            real += values[start + index] * std::cos(phase);
            imaginary -= values[start + index] * std::sin(phase);
        }
        return std::sqrt(real * real + imaginary * imaginary);
    };

    const auto fundamental = magnitude(frequency);
    auto harmonicsSquared = 0.0;
    for (auto harmonic = 2; harmonic <= 5 && harmonic * frequency < audio.sampleRate * 0.5; ++harmonic)
    {
        const auto value = magnitude(harmonic * frequency);
        harmonicsSquared += value * value;
    }
    return toDb(std::sqrt(harmonicsSquared) / std::max(fundamental, 1.0e-16));
}

double sineFundamentalPeak(const AudioFile& audio, const double frequency)
{
    const auto start = std::min(audio.samples.getNumSamples() / 4,
                                static_cast<int>(audio.sampleRate * 0.5));
    const auto count = std::min(static_cast<int>(audio.sampleRate), audio.samples.getNumSamples() - start);
    if (count < 64 || frequency <= 0.0 || frequency >= audio.sampleRate * 0.5)
        return 0.0;

    auto real = 0.0;
    auto imaginary = 0.0;
    const auto* values = audio.samples.getReadPointer(0);
    for (auto index = 0; index < count; ++index)
    {
        const auto phase = 2.0 * juce::MathConstants<double>::pi * frequency * index / audio.sampleRate;
        real += values[start + index] * std::cos(phase);
        imaginary -= values[start + index] * std::sin(phase);
    }
    return 2.0 * std::sqrt(real * real + imaginary * imaginary) / count;
}

double peak(const AudioFile& audio, const int channel = 0)
{
    const auto* values = audio.samples.getReadPointer(
        std::clamp(channel, 0, audio.samples.getNumChannels() - 1));
    auto result = 0.0;
    for (auto sample = 0; sample < audio.samples.getNumSamples(); ++sample)
        result = std::max(result, std::abs(static_cast<double>(values[sample])));
    return result;
}

double frequencyFromName(const std::string& name)
{
    const auto marker = name.find("_F");
    const auto unit = name.find("Hz", marker);
    if (marker == std::string::npos || unit == std::string::npos)
        return 0.0;
    try { return std::stod(name.substr(marker + 2, unit - marker - 2)); }
    catch (...) { return 0.0; }
}

void analyseBiteSeries(const juce::File& directory, const std::string& testPrefix)
{
    std::map<std::string, juce::File> biteOne;
    std::map<std::string, juce::File> biteFive;
    std::map<std::string, juce::File> biteTen;
    for (const auto& file : wavFiles(directory))
    {
        const auto name = file.getFileName().toStdString();
        const auto separator = name.find(" - ");
        if (separator == std::string::npos || name.find(testPrefix) == std::string::npos)
            continue;
        const auto key = name.substr(separator + 3);
        const auto prefix = name.substr(0, separator);
        if (hasCsvToken(prefix, "bite1")) biteOne[key] = file;
        if (hasCsvToken(prefix, "bite5")) biteFive[key] = file;
        if (hasCsvToken(prefix, "bite10")) biteTen[key] = file;
    }

    std::cout << "\nBITE_SERIES,test,pairs,bite5_vs_1_error_db,bite10_vs_1_error_db\n";
    auto pairsFive = 0;
    auto pairsTen = 0;
    auto sumFive = 0.0;
    auto sumTen = 0.0;
    for (const auto& [key, first] : biteOne)
    {
        const auto reference = readAudio(first);
        if (const auto it = biteFive.find(key); it != biteFive.end())
        {
            sumFive += errorDb(readAudio(it->second), reference);
            ++pairsFive;
        }
        if (const auto it = biteTen.find(key); it != biteTen.end())
        {
            sumTen += errorDb(readAudio(it->second), reference);
            ++pairsTen;
        }
    }
    std::cout << testPrefix << ',' << biteOne.size() << ',' << pairsFive << ','
              << (pairsFive > 0 ? sumFive / pairsFive : 0.0) << ','
              << (pairsTen > 0 ? sumTen / pairsTen : 0.0) << '\n';
}

void analyseSaturation(const juce::File& directory)
{
    struct SaturationMeasurement
    {
        GainRange gain;
        double inputThd {};
        double outputThd {};
    };
    std::map<std::string, std::vector<SaturationMeasurement>> measurements;
    for (const auto& file : wavFiles(directory))
    {
        const auto name = file.getFileName().toStdString();
        if (!name.starts_with("pro - "))
            continue;
        const auto sourceName = name.substr(6);
        const auto sourceFile = directory.getChildFile(juce::String(sourceName));
        if (!sourceFile.existsAsFile())
            throw std::runtime_error("Missing saturation source for " + name);
        const auto frequency = frequencyFromName(name);
        const auto marker = name.find("_F");
        const auto gain = marker != std::string::npos
            ? name.substr(name.find("T06_"), marker - name.find("T06_")) : "unknown";
        if (frequency > 0.0)
        {
            const auto source = readAudio(sourceFile);
            const auto render = readAudio(file);
            measurements[gain].push_back({ gainRange(source, render),
                                           sineThdDb(source, frequency),
                                           sineThdDb(render, frequency) });
        }
    }

    std::cout << "\nSATURATION_PAIRS,input_gain_id,files,min_gain_db,max_gain_db,"
                 "min_input_thd_db,max_input_thd_db,min_output_thd_db,max_output_thd_db\n";
    for (const auto& [gain, values] : measurements)
    {
        auto minGain = std::numeric_limits<double>::infinity();
        auto maxGain = -std::numeric_limits<double>::infinity();
        auto minInputThd = std::numeric_limits<double>::infinity();
        auto maxInputThd = -std::numeric_limits<double>::infinity();
        auto minOutputThd = std::numeric_limits<double>::infinity();
        auto maxOutputThd = -std::numeric_limits<double>::infinity();
        for (const auto& value : values)
        {
            minGain = std::min(minGain, value.gain.minimum);
            maxGain = std::max(maxGain, value.gain.maximum);
            minInputThd = std::min(minInputThd, value.inputThd);
            maxInputThd = std::max(maxInputThd, value.inputThd);
            minOutputThd = std::min(minOutputThd, value.outputThd);
            maxOutputThd = std::max(maxOutputThd, value.outputThd);
        }
        std::cout << gain << ',' << values.size() << ',' << minGain << ',' << maxGain << ','
                  << minInputThd << ',' << maxInputThd << ',' << minOutputThd << ','
                  << maxOutputThd << '\n';
    }

    std::cout << "SATURATION_DETAIL,id,input_fund_peak_dbfs,output_fund_peak_dbfs,"
                 "output_peak_dbfs,steady_fund_gain_db,output_thd_db\n";
    for (const auto& file : wavFiles(directory))
    {
        const auto name = file.getFileName().toStdString();
        if (!name.starts_with("pro - ") || name.find("_F") == std::string::npos)
            continue;
        const auto sourceFile = directory.getChildFile(juce::String(name.substr(6)));
        if (!sourceFile.existsAsFile()) continue;
        const auto frequency = frequencyFromName(name);
        if (frequency <= 0.0) continue;
        const auto source = readAudio(sourceFile);
        const auto render = readAudio(file);
        const auto inputFundamental = sineFundamentalPeak(source, frequency);
        const auto outputFundamental = sineFundamentalPeak(render, frequency);
        std::cout << name << ',' << toDb(inputFundamental) << ',' << toDb(outputFundamental)
                  << ',' << toDb(peak(render)) << ','
                  << toDb(outputFundamental / std::max(inputFundamental, 1.0e-16))
                  << ',' << sineThdDb(render, frequency) << '\n';
    }
}

void analysePrefixedPairs(const juce::File& directory, const std::string& testPrefix)
{
    std::map<std::string, std::vector<GainRange>> groups;
    auto complete = 0;
    for (const auto& renderFile : wavFiles(directory))
    {
        const auto name = renderFile.getFileName().toStdString();
        const auto separator = name.find(" - ");
        if (separator == std::string::npos || name.find(testPrefix) == std::string::npos)
            continue;
        const auto sourceName = name.substr(separator + 3);
        const auto sourceFile = directory.getChildFile(juce::String(sourceName));
        if (!sourceFile.existsAsFile())
            continue;
        const auto group = name.substr(0, separator);
        groups[group].push_back(gainRange(readAudio(sourceFile), readAudio(renderFile)));
        ++complete;
    }

    std::cout << "\nPREFIXED_PAIRS,test,prefix,pairs,min_gain_db,max_gain_db,mean_gain_db\n";
    for (const auto& [prefix, ranges] : groups)
    {
        auto minimum = std::numeric_limits<double>::infinity();
        auto maximum = -std::numeric_limits<double>::infinity();
        auto mean = 0.0;
        for (const auto& range : ranges)
        {
            minimum = std::min(minimum, range.minimum);
            maximum = std::max(maximum, range.maximum);
            mean += range.mean;
        }
        std::cout << testPrefix << ',' << prefix << ',' << ranges.size() << ','
                  << minimum << ',' << maximum << ',' << mean / ranges.size() << '\n';
    }
    std::cout << "PREFIXED_PAIRS_TOTAL," << testPrefix << ',' << complete << '\n';
}

void analyseAutoManualPairs(const juce::File& directory)
{
    std::cout << "\nAUTO_MANUAL_PAIRS,id,mc303_min_max_error_db,mc404_min_max_error_db\n";
    for (const auto& source : wavFiles(directory))
    {
        const auto name = source.getFileName().toStdString();
        if (!name.starts_with("T08_")) continue;
        const auto mc303Min = directory.getChildFile("303A_Rmin - " + juce::String(name));
        const auto mc303Max = directory.getChildFile("303A_Rmax - " + juce::String(name));
        const auto mc404Min = directory.getChildFile("404A_Rmin - " + juce::String(name));
        const auto mc404Max = directory.getChildFile("404A_Rmax - " + juce::String(name));
        if (!mc303Min.existsAsFile() || !mc303Max.existsAsFile()
            || !mc404Min.existsAsFile() || !mc404Max.existsAsFile())
            throw std::runtime_error("Incomplete T08 Auto manual pair for " + name);
        std::cout << name << ',' << errorDb(readAudio(mc303Min), readAudio(mc303Max)) << ','
                  << errorDb(readAudio(mc404Min), readAudio(mc404Max)) << '\n';
    }
}

void analyseStereoPairs(const juce::File& directory)
{
    auto maximumChannelDifference = 0.0;
    auto pairs = 0;
    for (const auto& renderFile : wavFiles(directory))
    {
        const auto name = renderFile.getFileName().toStdString();
        const auto separator = name.find(" - ");
        if (separator == std::string::npos || name.find("T09_") == std::string::npos)
            continue;
        const auto sourceFile = directory.getChildFile(juce::String(name.substr(separator + 3)));
        if (!sourceFile.existsAsFile()) continue;
        const auto source = readAudio(sourceFile);
        const auto render = readAudio(renderFile);
        const auto left = gainRange(source, render, 0).mean;
        const auto right = gainRange(source, render, 1).mean;
        maximumChannelDifference = std::max(maximumChannelDifference, std::abs(left - right));
        ++pairs;
    }
    std::cout << "\nSTEREO_DETECTOR,pairs,max_left_right_gain_difference_db\n"
              << pairs << ',' << maximumChannelDifference << '\n';
}

void analyseAdditional(const juce::File& root)
{
    analyseAuto(root.getChildFile("02_Auto"));
    analyseProPairs(root.getChildFile("03_Dynamics_Validation"));
    analyseBiteSeries(root.getChildFile("04_BITE_Complete"), "T04_");
    analyseSaturation(root.getChildFile("06_Saturation_Headroom"));
}

void analyseIntegration(const juce::File& root)
{
    analysePrefixedPairs(root.getChildFile("08_Auto_Internal_Bands"), "T08_");
    analyseAutoManualPairs(root.getChildFile("08_Auto_Internal_Bands"));
    analysePrefixedPairs(root.getChildFile("09_Stereo_Detector_Internal_Bands"), "T09_");
    analyseStereoPairs(root.getChildFile("09_Stereo_Detector_Internal_Bands"));
    analysePrefixedPairs(root.getChildFile("10_InterBand_Dynamic_Independence"), "T10_");
    analysePrefixedPairs(root.getChildFile("11_BITE_Internal_Bands"), "T11_");
    analyseBiteSeries(root.getChildFile("11_BITE_Internal_Bands"), "T11_");
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: MC2000ExtendedAnalysis <additional-pack-root> <integration-pack-root>\n";
            return 2;
        }
        const juce::File additional(juce::String::fromUTF8(argv[1]));
        const juce::File integration(juce::String::fromUTF8(argv[2]));
        if (!additional.isDirectory() || !integration.isDirectory())
            throw std::runtime_error("Both arguments must be test-pack roots");
        std::cout << std::fixed << std::setprecision(6);
        analyseAdditional(additional);
        analyseIntegration(integration);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Extended analysis failed: " << error.what() << '\n';
        return 1;
    }
}
