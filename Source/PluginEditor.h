#pragma once

#include "PluginProcessor.h"
#include "PonteLookAndFeel.h"
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
    void mouseDown(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void focusOfChildComponentChanged(FocusChangeType) override;

private:
    void timerCallback() override;
    void showValueForInteraction();
    void setValueVisible(bool visible);
    double hoverStartedMs {};
    double visibleUntilMs {};
    bool hovering {};
    bool dragging {};
    juce::Label name;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ContextHeader final : public juce::Component
{
public:
    void setHelpText(const juce::String& text);
    void setBandCount(int count);
    void paint(juce::Graphics&) override;

private:
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

private:
    PonteMC2000AudioProcessor& processor;
    int band {};
};

class BandStrip final : public juce::Component
{
public:
    BandStrip(PonteMC2000AudioProcessor&, int bandIndex);
    void paint(juce::Graphics&) override;
    void resized() override;
    void setActiveVisual(bool active);
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
    void paint(juce::Graphics&) override;
    void updateSpectrum();
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    float frequencyToX(double frequency) const noexcept;
    double xToFrequency(float x) const noexcept;
    int currentBandCount() const noexcept;
    bool bandIsAudible(int band) const noexcept;
    int bandForFrequency(double frequency) const noexcept;
    PonteMC2000AudioProcessor& processor;
    int draggedCrossover { -1 };
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> fftWindow {
        fftSize, juce::dsp::WindowingFunction<float>::hann, true };
    std::array<float, fftSize> fftInput {};
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

private:
    PonteMC2000AudioProcessor& processor;
    int foregroundBand { 0 };
};

class OutputMeter final : public juce::Component
{
public:
    explicit OutputMeter(PonteMC2000AudioProcessor& p) : processor(p) {}
    void paint(juce::Graphics&) override;

private:
    PonteMC2000AudioProcessor& processor;
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
    juce::String activeHelpText;
    int displayedBandCount { 4 };
    static constexpr int linkedControlCount = 7;
    int displayedLinkMaster { -1 };
    bool linkDisplayInitialised {};
    std::array<std::array<double, linkedControlCount>, 4> linkDisplayOffsets {};
    std::array<std::array<double, linkedControlCount>, 4> previousLinkedValues {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PonteMC2000AudioProcessorEditor)
};
