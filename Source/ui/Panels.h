#pragma once

#include "Controls.h"
#include "FilterIcons.h"
#include "../dsp/EqEngine.h"

class MeridianProcessor;

/** Rounded panel background with a caption. */
class PanelBase : public juce::Component
{
public:
    void paintPanel (juce::Graphics& g);
};

/** Preset name with previous / next, a menu and save. */
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (MeridianProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void showMenu();
    void saveAs();
    void loadFromFile();

    MeridianProcessor& proc;
    IconButton prev { "Previous preset", icons::chevronLeft() };
    IconButton next { "Next preset", icons::chevronRight() };
    IconButton save { "Save preset", icons::save() };
    juce::Rectangle<int> nameArea;
    std::unique_ptr<juce::FileChooser> chooser;
};

class HeaderBar : public juce::Component
{
public:
    explicit HeaderBar (MeridianProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override { if (onBackgroundClick) onBackgroundClick(); }

    /** Refresh undo / redo availability and the preset name. */
    void update();

    std::function<void()> onSettings, onUndo, onRedo, onBackgroundClick;

private:
    MeridianProcessor& proc;
    IconButton undo { "Undo (Ctrl/Cmd+Z)", icons::undo() };
    IconButton redo { "Redo (Ctrl/Cmd+Shift+Z)", icons::redo() };
    PresetBar presetBar;
    SegmentedControl phase { { "Zero Latency", "Analog", "Linear", "Mixed" } };
    juce::ToggleButton oversampling { juce::String (juce::CharPointer_UTF8 ("2\xc3\x97 Oversampling")) };
    IconButton settings { "Options", icons::gear() };
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
    static juce::String chipLabel (int band, const meq::BandParams&);

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
    TypeSelector type;
    Knob freq { "Frequency" }, gain { "Gain", false, true }, q { "Q" }, slope { "Slope" };
    SegmentedControl channel { { "Stereo", "L", "R", "M", "S" } };
    juce::TextButton solo { "S" };
    IconButton bypass { "Bypass band", icons::power() };
    IconButton remove { "Delete band", icons::cross() };
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
    void mouseDown (const juce::MouseEvent&) override { if (onBackgroundClick) onBackgroundClick(); }

    /** Set by the editor: which renderer is drawing the interface right now. */
    std::function<juce::String()> rendererName;
    std::function<void()> onBackgroundClick;

private:
    MeridianProcessor& proc;
    BarSlider gainScale { "Gain scale", false, true }, width { "Stereo width" }, output { "Output", false, true },
              lookahead { "Lookahead" };
    juce::ToggleButton autoGain { "Auto gain" };
    SegmentedControl monitor { { "Off", "Mid", "Side" } };
    std::unique_ptr<juce::ButtonParameterAttachment> autoAttachment;
    juce::String status;
};
