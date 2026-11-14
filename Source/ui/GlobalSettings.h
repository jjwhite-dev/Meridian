#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include "Theme.h"

/** Interface preferences shared by every Meridian EQ instance and kept in a
    settings file (not in the session): display options, interface size,
    renderer and colours. Use through juce::SharedResourcePointer. */
class GlobalSettings : public juce::ChangeBroadcaster
{
public:
    GlobalSettings();
    ~GlobalSettings() override;

    int refreshHz = 60;
    int fftOrder = 13;        // High
    float brightness = 1.0f;
    float dbRange = 18.0f;
    bool gpu = true;
    float uiScale = 1.0f;
    int logicalWidth = 1240, logicalHeight = 800;
    int paletteIndex = 0;     // -1 = custom
    theme::Palette palette;

    /** Saves, re-applies the palette and tells every open editor. */
    void changed();
    void save();

    static const std::array<float, 6>& uiScales() { static const std::array<float, 6> s { 0.8f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f }; return s; }

private:
    std::unique_ptr<juce::PropertiesFile> file;
};
