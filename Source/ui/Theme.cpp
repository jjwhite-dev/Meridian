#include "Theme.h"

namespace theme
{

const std::vector<Slot>& slots()
{
    static const std::vector<Slot> s {
        { "bg",       "Background",          &Palette::bg },
        { "panel",    "Panels",              &Palette::panel },
        { "display",  "Display",             &Palette::display },
        { "text",     "Text",                &Palette::text },
        { "muted",    "Secondary text",      &Palette::muted },
        { "accent",   "Accent",              &Palette::accent },
        { "amber",    "Dynamics",            &Palette::amber },
        { "stereo",   "Stereo bands",        &Palette::stereo },
        { "left",     "Left bands",          &Palette::left },
        { "right",    "Right bands",         &Palette::right },
        { "mid",      "Mid bands",           &Palette::mid },
        { "side",     "Side bands",          &Palette::side },
        { "specLow",  "Spectrum: lows",      &Palette::specLow },
        { "specHigh", "Spectrum: highs",     &Palette::specHigh },
    };
    return s;
}

namespace
{
    juce::Colour c (juce::uint32 rgb) { return juce::Colour (0xff000000 | rgb); }

    Palette make (const char* name, juce::uint32 bg, juce::uint32 panel, juce::uint32 display, juce::uint32 text,
                  juce::uint32 muted, juce::uint32 accent, juce::uint32 amber,
                  juce::uint32 stereo, juce::uint32 left, juce::uint32 right, juce::uint32 mid, juce::uint32 side,
                  juce::uint32 specLow, juce::uint32 specHigh, float fill)
    {
        return { name, c (bg), c (panel), c (display), c (text), c (muted), c (accent), c (amber),
                 c (stereo), c (left), c (right), c (mid), c (side), c (specLow), c (specHigh), fill };
    }
}

const std::vector<Palette>& presets()
{
    static const std::vector<Palette> p {
        make ("Graphite",      0x0b0c0f, 0x121418, 0x0e1014, 0xe7e9ee, 0x8d93a0, 0x6ccbf5, 0xf0a35e,
                               0x6ccbf5, 0x5b8cff, 0xff8fb1, 0x8fe3a6, 0xc39bff, 0x46708c, 0x7a5ba8, 0.14f),
        make ("Midnight",      0x0a0e1a, 0x10162a, 0x0c1222, 0xe6eaf5, 0x8a93b0, 0x8fa8ff, 0xffb36b,
                               0x8fa8ff, 0x5ec8ff, 0xff8fc8, 0x7fe0b4, 0xd0a4ff, 0x30508f, 0x6a3f9a, 0.16f),
        make ("Nordic Frost",  0x0f1418, 0x151c22, 0x11181d, 0xe5edf1, 0x8b9ba6, 0x88c0d0, 0xebcb8b,
                               0x88c0d0, 0x81a1c1, 0xe3a0b8, 0xa3be8c, 0xb48ead, 0x3b6a78, 0x5e81ac, 0.15f),
        make ("Obsidian Gold", 0x0b0b0c, 0x141414, 0x0f0f10, 0xece8df, 0x9a9488, 0xd9b56c, 0xe07a5f,
                               0xd9b56c, 0x7fa6d9, 0xe89fb5, 0x9ccc9a, 0xb9a0e0, 0x5e5030, 0x8f6f3a, 0.14f),
        make ("Rose Quartz",   0x140e12, 0x1c1419, 0x171015, 0xf3e9ee, 0xa5939c, 0xf2a7c3, 0xf5c07a,
                               0xf2a7c3, 0x8fb3ff, 0xff9a8a, 0x9fe0c0, 0xc9a7ff, 0x7a3e5a, 0x6a4a9a, 0.15f),
        make ("Forest",        0x0c120f, 0x121a16, 0x0e1612, 0xe6efe9, 0x8ea197, 0x7fdcb0, 0xf0b45e,
                               0x7fdcb0, 0x6fa8ff, 0xff9fb0, 0xc8e07f, 0xb8a0ff, 0x2f6a4f, 0x5a7a2a, 0.15f),
        make ("Neon Noir",     0x07070a, 0x0f0f15, 0x0a0a10, 0xececf4, 0x8c8ca3, 0xff4fd8, 0xffb020,
                               0xff4fd8, 0x4fd8ff, 0xff6b6b, 0x7dff9b, 0xb07dff, 0x2a2f7a, 0x7a1f6a, 0.18f),
        make ("Solar Ember",   0x120c08, 0x1b130e, 0x160f0b, 0xf2e8df, 0xa39485, 0xff8a3d, 0xffd166,
                               0xff8a3d, 0x6fa8ff, 0xff7aa2, 0x9fd67a, 0xc3a0ff, 0x6a3214, 0x8a5a1a, 0.16f),
        make ("Nexus",         0x050506, 0x0d0c10, 0x050506, 0xe9e6ee, 0x8f8a99, 0xff8a3d, 0xffd166,
                               0xff8a3d, 0x8fa8ff, 0xff6fa8, 0x7fe0b4, 0xc39bff, 0xff3fd2, 0xff8a3d, 0.18f),
        make ("Sandstone",     0xe9e4dc, 0xf7f3ee, 0xf2ede6, 0x2a2622, 0x6e655b, 0xb4561f, 0x9a6b00,
                               0xb4561f, 0x2f6fd6, 0xc0407a, 0x2f8f5a, 0x7a4fc0, 0xc9b49a, 0xb48a6a, 0.30f),
        make ("Arctic",        0xe6eaf0, 0xfbfcfd, 0xf3f6f9, 0x1d2430, 0x5c6676, 0x2b7fd9, 0xc8701e,
                               0x2b7fd9, 0x4b5fd6, 0xd6457a, 0x23966a, 0x8a52d6, 0xa9c0de, 0xb2a6dc, 0.32f),
    };
    return p;
}

void apply (const Palette& p)
{
    const bool light = p.bg.getPerceivedBrightness() > 0.5f;
    auto mixPanel = [&] (float t) { return p.panel.interpolatedWith (p.text, t); };

    bg = p.bg;
    panel = p.panel;
    display = p.display;
    text = p.text;
    muted = p.muted;
    accent = p.accent;
    amber = p.amber;

    well = mixPanel (light ? 0.05f : 0.05f);
    line = mixPanel (light ? 0.08f : 0.05f);
    border = mixPanel (light ? 0.14f : 0.09f);
    border2 = mixPanel (light ? 0.22f : 0.14f);
    gridMinor = p.display.interpolatedWith (p.text, light ? 0.05f : 0.035f);
    gridMajor = p.display.interpolatedWith (p.text, light ? 0.10f : 0.075f);
    zeroLine = p.display.interpolatedWith (p.text, light ? 0.22f : 0.15f);
    textDim = p.text.interpolatedWith (p.muted, 0.3f);
    faint = p.panel.interpolatedWith (p.muted, 0.55f);
    onAccent = on (p.accent);
    preLine = p.display.interpolatedWith (p.muted, light ? 0.35f : 0.22f);
    banner = p.panel.interpolatedWith (p.accent, 0.14f);
    dim = p.bg.withAlpha (0.66f);
    nodeFill = p.display.interpolatedWith (p.text, 0.06f);
    specLow = p.specLow;
    specHigh = p.specHigh;
    specFill = p.specFill;
    channels = { p.stereo, p.left, p.right, p.mid, p.side };
    ++version;
}

} // namespace theme
