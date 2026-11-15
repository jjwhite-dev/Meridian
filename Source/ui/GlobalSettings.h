#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include "Theme.h"

/** Interface options shared by every Q-TIP instance.
    At startup they are loaded from the saved defaults (or the factory
    defaults). Changes apply right away but are not remembered for the next
    startup unless the user chooses "Save as default".
    Use through juce::SharedResourcePointer (the processor holds one, so the
    options live as long as the plugin is loaded, not only while a window is open). */
class GlobalSettings : public juce::ChangeBroadcaster
{
public:
    GlobalSettings();

    int refreshHz = 60;
    int fftOrder = 13;        // High
    float brightness = 1.0f;
    int spectrumStyle = 0;    // 0 smooth, 1 particles
    float particleDot = 2.4f;
    int particleSpacing = 6;
    bool spectrumGrab = false;
    float dbRange = 18.0f;
    bool gpu = true;
    float uiScale = 1.0f;
    int logicalWidth = 1240, logicalHeight = 800;
    int paletteIndex = 0;     // -1 = custom
    theme::Palette palette;

    /** Re-applies the palette and tells every open editor (not saved). */
    void changed();

    /** Makes the current options the startup defaults. */
    void saveAsDefaults();
    /** Back to the factory options, and forgets the saved defaults. */
    void resetToFactory();

    static const std::array<float, 6>& uiScales() { static const std::array<float, 6> s { 0.8f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f }; return s; }

private:
    void loadFrom (juce::PropertiesFile&);
    void setFactory();
    std::unique_ptr<juce::PropertiesFile> file;
};
