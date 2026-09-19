#pragma once

#include "PluginProcessor.h"
#include "PonteLookAndFeel.h"
#include "UI/ControlFocus.h"
#include "UI/MeterBallistics.h"
#include "UI/ReleaseCheck.h"
#include <array>
#include <functional>
#include <juce_dsp/juce_dsp.h>

class ParameterKnob final : public juce::Component,
                            private juce::Timer
{
public:
    ParameterKnob(juce::AudioProcessorValueTreeState&, const juce::String& parameterId,
                  const juce::String& caption, const juce::String& suffix = {},
                  const juce::String& helpText = {}, int decimalPlaces = 2);
    void resized() override;
    void parentHierarchyChanged() override;
    void mouseDown(const juce::MouseEvent&) override;
    void registerFocus(pontedsp::gui::ControlFocus&);
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void setValueVisible(bool visible);
    void positionValueDisplay();
    void updateValueText();
    bool dragging {};
    juce::Label name;
    juce::Slider slider;
    juce::Label valueDisplay;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ContextHeader final : public juce::Component, private juce::Timer
{
public:
    ContextHeader();
    void setHelpText(const juce::String& text);
    void setBandCount(int count);
    void setLatestVersion(const juce::String& version);
    juce::String versionText() const;
    bool isShowingHelp() const noexcept { return helpText.isNotEmpty(); }
    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;
    juce::SharedResourcePointer<pontedsp::gui::ReleaseCheck> releaseCheck;
    juce::String availableVersion;
    bool yellow {};
    juce::String helpText;
    int bandCount { 4 };
};

class CrossoverField final : public juce::Component,
                             private juce::Label::Listener,
                             private juce::Timer
{
public:
    CrossoverField(juce::AudioProcessorValueTreeState&, int crossoverIndex);
    ~CrossoverField() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    bool isEditing() const { return value.isBeingEdited(); }

private:
    void labelTextChanged(juce::Label*) override;
    void editorShown(juce::Label*, juce::TextEditor&) override;
    void timerCallback() override;
    double constrainedFrequency(double requested) const noexcept;
    static double valueFromText(const juce::String&);
    static juce::String textFromValue(double);

    juce::AudioProcessorValueTreeState& state;
    int index {};
    bool updating {};
    juce::Label value;
};

class BandMeter final : public juce::Component
{
public:
    BandMeter(PonteMC2000AudioProcessor&, int bandIndex);
    void paint(juce::Graphics&) override;
    void update(double elapsedSeconds);
    pontedsp::mc2000::dsp::BandMeterSnapshot displayedValues() const noexcept { return snapshot; }

private:
    PonteMC2000AudioProcessor& processor;
    int band {};
    pontedsp::gui::LevelMeterBallistics inputBallistics, outputBallistics;
    pontedsp::gui::GainReductionMeterBallistics grBallistics;
    pontedsp::mc2000::dsp::BandMeterSnapshot snapshot;
};

class BandStrip final : public juce::Component
{
public:
    BandStrip(PonteMC2000AudioProcessor&, int bandIndex);
    void paint(juce::Graphics&) override;
    void resized() override;
    void setActiveVisual(bool active);
    void updateMeters(double elapsedSeconds) { meter.update(elapsedSeconds); }
    pontedsp::mc2000::dsp::BandMeterSnapshot displayedMeters() const noexcept { return meter.displayedValues(); }
    void mouseDown(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void focusOfChildComponentChanged(FocusChangeType) override;
    std::function<void()> onInteraction;

private:
    PonteMC2000AudioProcessor& processor;
    int band {};
    juce::Label title, algorithmLabel;
    juce::TextButton enabled { "IN" }, solo { "SOLO" };
    ParameterKnob gain, threshold, ratio, knee, bite, attack, release;
    juce::ComboBox timeConstant;
    BandMeter meter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment, soloAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> timeConstantAttachment;
    bool activeVisual { true };
};

class CrossoverPlot final : public juce::Component
{
public:
    explicit CrossoverPlot(PonteMC2000AudioProcessor&);
    ~CrossoverPlot() override;
    void paint(juce::Graphics&) override;
    void updateSpectrum(double elapsedSeconds);
    double inputResponseDb(double frequency) const noexcept;
    float displayedSpectrumDb(int bin) const noexcept { return spectrumDb[static_cast<std::size_t>(bin)]; }
    static constexpr int spectrumSize = 2048;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    float frequencyToX(double frequency) const noexcept;
    double xToFrequency(float x) const noexcept;
    int currentBandCount() const noexcept;
    bool bandIsAudible(int band) const noexcept;

    PonteMC2000AudioProcessor& processor;
    int draggedCrossover { -1 };
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> fftWindow {
        fftSize, juce::dsp::WindowingFunction<float>::hann, true };
    std::array<std::array<float, fftSize>, 2> fftInput {};
    std::array<PonteMC2000AudioProcessor::SpectrumSample, 16384> incoming {};
    std::array<pontedsp::gui::LevelMeterBallistics, fftSize / 2> spectrumBallistics;
    std::array<float, fftSize / 2> latestSpectrumDb {};
    pontedsp::mc2000::dsp::CrossoverNetwork displayResponse;
    std::array<bool, 4> inputBands { true, true, true, true };
    int responseBandCount { 4 };
    double withoutSpectrumSamplesSeconds {};
    std::array<double, fftSize / 2> responseDb {};
    std::array<double, 3> cachedResponseFrequencies {};
    std::array<bool, 4> cachedInputBands {};
    int cachedResponseBandCount {};
    double cachedResponseSampleRate {};
    std::array<float, fftSize * 2> fftWork {};
    std::array<float, fftSize / 2> spectrumDb {};
    int fftInputCount {};
    bool spectrumReady {};
};

class CompressionPlot final : public juce::Component
{
public:
    explicit CompressionPlot(PonteMC2000AudioProcessor& p) : processor(p) {}
    void paint(juce::Graphics&) override;
    void setForegroundBand(int band);
    void setDisplayedMeters(const std::array<pontedsp::mc2000::dsp::BandMeterSnapshot, 4>& values) { displayedMeters = values; }
    float displayedInputDb(int band) const { return displayedMeters[static_cast<std::size_t>(band)].inputDb; }

private:
    PonteMC2000AudioProcessor& processor;
    int foregroundBand { 0 };
    std::array<pontedsp::mc2000::dsp::BandMeterSnapshot, 4> displayedMeters;
};

class OutputMeter final : public juce::Component
{
public:
    explicit OutputMeter(PonteMC2000AudioProcessor& p) : processor(p) {}
    void paint(juce::Graphics&) override;
    void update(double elapsedSeconds);
    std::array<float, 2> displayedValues() const noexcept { return levels; }

private:
    PonteMC2000AudioProcessor& processor;
    std::array<pontedsp::gui::LevelMeterBallistics, 2> ballistics;
    std::array<float, 2> levels { -100.0f, -100.0f };
};

class PonteMC2000AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit PonteMC2000AudioProcessorEditor(PonteMC2000AudioProcessor&);
    ~PonteMC2000AudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    int activeBandCount() const noexcept;
    void updateContextHelp();
    void updateBandCountLayout();
    void updateLinkedControls();
    void updateBandVisualStates();

    PonteMC2000AudioProcessor& processor;
    pontedsp::gui::PonteLookAndFeel lookAndFeel;
    ContextHeader contextHeader;
    ParameterKnob inputGain, outputGain;
    juce::TextButton phase { "PHASE" };
    juce::ComboBox bandCount, linkMaster;
    juce::Label crossoverLabel, bandCountLabel, linkLabel;
    CrossoverPlot crossoverPlot;
    std::array<std::unique_ptr<CrossoverField>, 3> crossoverFields;
    CompressionPlot compressionPlot;
    OutputMeter outputMeter;
    std::array<std::unique_ptr<BandStrip>, 4> bands;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> phaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bandCountAttachment, linkAttachment;
    juce::Component* hoverHelpTarget {};
    juce::Point<int> lastMousePosition;
    double hoverHelpStartedMs {};
    double lastMeterUpdateMs {};
    juce::String activeHelpText;
    int displayedBandCount { 4 };
    static constexpr int linkedControlCount = 7;
    int displayedLinkMaster { -1 };
    bool linkDisplayInitialised {};
    std::array<std::array<double, linkedControlCount>, 4> linkDisplayOffsets {};
    std::array<std::array<double, linkedControlCount>, 4> previousLinkedValues {};
    // Destroy before the controls captured by its callbacks.
    std::unique_ptr<pontedsp::gui::ControlFocus> controlFocus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PonteMC2000AudioProcessorEditor)
};
