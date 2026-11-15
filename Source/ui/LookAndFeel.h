#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Slider properties understood by the look and feel:
      "bipolar"  (bool)  draw the value arc / bar from the centre
      "amber"    (bool)  use the dynamics colour
    ToggleButton property:
      "switch"   (bool)  draw as an on/off switch instead of a pill */
class QTipLookAndFeel : public juce::LookAndFeel_V4
{
public:
    QTipLookAndFeel();
    /** Re-reads the theme colours (after the palette changed). */
    void applyTheme();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};
