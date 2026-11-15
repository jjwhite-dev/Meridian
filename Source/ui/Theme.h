#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** The interface colours. A Palette holds the colours a user can edit; apply()
    derives the rest (borders, grid, ...) and sets the globals below, which the
    whole interface paints with. The palette is a user preference shared by all
    instances (see GlobalSettings). */
namespace theme
{
    struct Palette
    {
        juce::String name;
        juce::Colour bg, panel, display, text, muted, accent, amber;
        juce::Colour stereo, left, right, mid, side;
        juce::Colour specLow, specHigh;   // analyzer gradient, low to high frequencies
        float specFill = 0.14f;           // analyzer fill opacity
    };

    /** Editable colour slots, in the order the options panel lists them. */
    struct Slot { const char* key; const char* label; juce::Colour Palette::* member; };
    const std::vector<Slot>& slots();

    /** The built-in palettes. */
    const std::vector<Palette>& presets();

    void apply (const Palette&);

    // ---- derived, current colours (set by apply)
    inline juce::Colour bg, panel, display, well, line, border, border2, gridMinor, gridMajor;
    inline juce::Colour text, textDim, muted, faint, accent, amber, onAccent;
    inline juce::Colour specLow, specHigh, preLine, zeroLine, banner, dim, nodeFill;
    inline float specFill = 0.14f;
    /** Incremented by apply(): lets cached drawings know the colours changed. */
    inline int version = 0;
    inline std::array<juce::Colour, 5> channels;

    /** One colour per processing channel: Stereo, Left, Right, Mid, Side. */
    inline juce::Colour channelColour (int channel) { return channels[(size_t) juce::jlimit (0, 4, channel)]; }

    /** Text colour that reads on top of a filled colour. */
    inline juce::Colour on (juce::Colour fill) { return fill.getPerceivedBrightness() > 0.55f ? juce::Colour (0xff0b0c0f) : juce::Colours::white; }

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
