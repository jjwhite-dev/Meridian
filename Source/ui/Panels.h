#pragma once

#include "Controls.h"

class MeridianProcessor;

/** Rounded panel background with a caption. */
class PanelBase : public juce::Component
{
public:
    void paintPanel (juce::Graphics& g);
};

class HeaderBar : public juce::Component
{
public:
    explicit HeaderBar (MeridianProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onSettings;

private:
    SegmentedControl phase { { "Zero Latency", "Analog", "Linear", "Mixed" } };
    juce::ToggleButton oversampling { juce::String (juce::CharPointer_UTF8 ("2\xc3\x97 Oversampling")) };
    IconButton settings { "GUI settings", icons::gear() };
    std::unique_ptr<juce::ButtonParameterAttachment> osAttachment;
};

class BandStrip : public juce::Component
{
public:
    explicit BandStrip (MeridianProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    int selected = -1;
    std::function<void (int)> onSelect;

private:
    struct Chip { int band; juce::Rectangle<float> r; };
    std::vector<Chip> layoutChips() const;
    juce::Rectangle<float> addButtonArea (const std::vector<Chip>&) const;

    MeridianProcessor& proc;
};

class BandPanel : public PanelBase
{
public:
    explicit BandPanel (MeridianProcessor&);
    void setBand (int band);
    void update();
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (int)> onSelect;

private:
    MeridianProcessor& proc;
    int band = -1;
    juce::ComboBox type;
    Knob freq { "Frequency" }, gain { "Gain", false, true }, q { "Q" }, slope { "Slope" };
    SegmentedControl channel { { "Stereo", "L", "R", "M", "S" } };
    juce::TextButton solo { "S" };
    IconButton bypass { "Bypass band", icons::power() };
    IconButton remove { "Delete band", icons::cross() };
    std::unique_ptr<juce::ComboBoxParameterAttachment> typeAttachment;
    std::unique_ptr<juce::ButtonParameterAttachment> bypassAttachment;
};

class DynamicsPanel : public PanelBase
{
public:
    explicit DynamicsPanel (MeridianProcessor&);
    void setBand (int band);
    void update();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MeridianProcessor& proc;
    int band = -1;
    juce::ToggleButton enable;
    SegmentedControl mode { { "Above", "Below", "Two-way" }, true };
    Knob threshold { "Threshold", true }, range { "Range", true, true }, ratio { "Ratio", true },
         attack { "Attack", true }, release { "Release", true };
    BarSlider onset { "Onset (transient)", true, true }, body { "Body (sustain)", true, true };
    SegmentedControl detect { { "Self", "Relative", "Sidechain" }, true };
    Knob detFreq { "Listen at", true }, detQ { "Width (Q)", true };
    std::unique_ptr<juce::ButtonParameterAttachment> enableAttachment;
    juce::String note;
    bool showControls = false;
};

class GlobalPanel : public PanelBase
{
public:
    explicit GlobalPanel (MeridianProcessor&);
    void update();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MeridianProcessor& proc;
    BarSlider gainScale { "Gain scale", false, true }, width { "Stereo width" }, output { "Output", false, true },
              lookahead { "Lookahead" };
    juce::ToggleButton autoGain { "Auto gain" };
    SegmentedControl monitor { { "Off", "Mid", "Side" } };
    std::unique_ptr<juce::ButtonParameterAttachment> autoAttachment;
    juce::String status;

public:
    /** Set by the editor: which renderer is drawing the interface right now. */
    std::function<juce::String()> rendererName;
};
