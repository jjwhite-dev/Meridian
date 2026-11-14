#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
    inline const juce::Colour bg      { 0xff0b0c0f };
    inline const juce::Colour panel   { 0xff121418 };
    inline const juce::Colour display { 0xff0e1014 };
    inline const juce::Colour well    { 0xff1a1d23 };
    inline const juce::Colour line    { 0xff1c1f26 };
    inline const juce::Colour border  { 0xff262a32 };
    inline const juce::Colour border2 { 0xff2e333d };
    inline const juce::Colour gridMinor { 0xff171a20 };
    inline const juce::Colour gridMajor { 0xff20242c };
    inline const juce::Colour text    { 0xffe7e9ee };
    inline const juce::Colour textDim { 0xffc9cdd5 };
    inline const juce::Colour muted   { 0xff8d93a0 };
    inline const juce::Colour faint   { 0xff4a505c };
    inline const juce::Colour accent  { 0xff6ccbf5 };
    inline const juce::Colour amber   { 0xfff0a35e };
    inline const juce::Colour onAccent { 0xff0b0c0f };

    /** One colour per processing channel: Stereo, Left, Right, Mid, Side.
        They differ in lightness as well as hue so they stay apart for colour-blind users. */
    inline juce::Colour channelColour (int channel)
    {
        switch (channel)
        {
            case 1:  return juce::Colour (0xff5b8cff); // Left: blue
            case 2:  return juce::Colour (0xffff8fb1); // Right: pink
            case 3:  return juce::Colour (0xff8fe3a6); // Mid: green
            case 4:  return juce::Colour (0xffc39bff); // Side: violet
            default: return accent;                    // Stereo: cyan
        }
    }

    inline juce::Font font (float height, bool bold = false)
    {
        auto opts = juce::FontOptions (height);
        if (bold) opts = opts.withStyle ("Bold");
        return juce::Font (opts);
    }

    inline juce::Font mono (float height)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
    }

    /** Small uppercase section caption. */
    inline void caption (juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r,
                         juce::Justification j = juce::Justification::centredLeft)
    {
        g.setColour (muted);
        g.setFont (font (11.0f, true));
        g.drawText (s.toUpperCase(), r, j, false);
    }
}
