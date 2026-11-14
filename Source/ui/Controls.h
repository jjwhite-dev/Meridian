#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

/** Rotary knob with its name above the value. Bind with attach(). */
class Knob : public juce::Component
{
public:
    explicit Knob (const juce::String& name, bool amber = false, bool bipolar = false);

    void attach (juce::RangedAudioParameter* p);
    void detach();
    void resized() override;
    void paint (juce::Graphics&) override;

    juce::Slider slider;

private:
    juce::String name;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
};

/** Horizontal bar slider with a caption and value on one line. */
class BarSlider : public juce::Component
{
public:
    explicit BarSlider (const juce::String& name, bool amber = false, bool bipolar = false);

    void attach (juce::RangedAudioParameter* p);
    void detach();
    void resized() override;
    void paint (juce::Graphics&) override;

    juce::Slider slider;

private:
    juce::String name;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
};

/** Row of mutually exclusive pills, bound to a choice (or any stepped) parameter,
    or used standalone through onChange. */
class SegmentedControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SegmentedControl (juce::StringArray options, bool amber = false);

    void attach (juce::RangedAudioParameter* p);
    void detach();

    void setSelected (int index, bool notify = false);
    int getSelected() const { return selected; }
    void setOptionEnabled (int index, bool enabled);
    /** Optional per-option colour for the selected pill (otherwise accent / amber). */
    void setOptionColours (juce::Array<juce::Colour> colours) { optionColours = std::move (colours); repaint(); }

    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    int indexAt (juce::Point<int>) const;
    juce::Rectangle<float> segment (int i) const;

    juce::StringArray options;
    juce::Array<bool> enabledFlags;
    juce::Array<juce::Colour> optionColours;
    int selected = 0, hover = -1;
    bool amber;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

/** Small square icon button drawn from a path. */
class IconButton : public juce::Button
{
public:
    IconButton (const juce::String& name, juce::Path icon);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::Path icon;
};

namespace icons
{
    juce::Path cross();
    juce::Path power();
    juce::Path gear();
    juce::Path speaker();
    juce::Path chevronLeft();
    juce::Path chevronRight();
    juce::Path save();
    juce::Path undo();
    juce::Path redo();
}
