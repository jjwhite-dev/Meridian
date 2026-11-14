#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/FilterDesign.h"

namespace filtericons
{
    /** The filter's typical curve (magnitude; phase for Allpass), fitted into `area`. */
    juce::Path curve (meq::FilterType type, juce::Rectangle<float> area);

    /** The icon as a Drawable, for popup menus. */
    std::unique_ptr<juce::Drawable> drawable (meq::FilterType type, juce::Colour colour);

    /** Draws a small framed icon. */
    void draw (juce::Graphics& g, meq::FilterType type, juce::Rectangle<float> area, juce::Colour colour, float thickness = 1.5f);

    /** Popup menu of all filter types with their icons. Item ids are 1 + type index. */
    juce::PopupMenu menu (int currentType, juce::Colour colour);
}

/** Shows the band's filter type as icon + name; click for a menu of all types
    with their curves. Bound to a choice parameter. */
class TypeSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    TypeSelector();
    void attach (juce::RangedAudioParameter* p);
    void detach();
    void setColour (juce::Colour c) { colour = c; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    int current = 0;
    juce::Colour colour;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};
