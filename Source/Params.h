#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/EqEngine.h"

namespace params
{
    // per-band parameter suffixes; full id is "b<1..32>_<suffix>"
    namespace band
    {
        inline constexpr const char* on      = "on";
        inline constexpr const char* type    = "type";
        inline constexpr const char* freq    = "freq";
        inline constexpr const char* gain    = "gain";
        inline constexpr const char* q       = "q";
        inline constexpr const char* slope   = "slope";
        inline constexpr const char* channel = "ch";
        inline constexpr const char* bypass  = "byp";
        inline constexpr const char* dyn     = "dyn";
        inline constexpr const char* dynMode = "dmode";
        inline constexpr const char* thr     = "thr";
        inline constexpr const char* range   = "range";
        inline constexpr const char* ratio   = "ratio";
        inline constexpr const char* attack  = "att";
        inline constexpr const char* release = "rel";
        inline constexpr const char* onset   = "onset";
        inline constexpr const char* body    = "body";
        inline constexpr const char* detect  = "det";
        inline constexpr const char* detFreq = "dfreq";
        inline constexpr const char* detQ    = "dq";

        inline constexpr const char* all[] = { on, type, freq, gain, q, slope, channel, bypass, dyn, dynMode,
                                               thr, range, ratio, attack, release, onset, body, detect, detFreq, detQ };
    }

    inline juce::String bandId (int bandIndex, const char* suffix)
    {
        return "b" + juce::String (bandIndex + 1) + "_" + suffix;
    }

    inline constexpr const char* phase     = "phase";
    inline constexpr const char* oversample = "os";
    inline constexpr const char* gainScale = "gscale";
    inline constexpr const char* output    = "out";
    inline constexpr const char* width     = "width";
    inline constexpr const char* autoGain  = "autog";
    inline constexpr const char* monitor   = "mon";
    inline constexpr const char* lookahead = "la";

    inline const juce::StringArray phaseModes { "Zero Latency", "Analog", "Linear", "Mixed" };
    inline const juce::StringArray channels   { "Stereo", "L", "R", "M", "S" };
    inline const juce::StringArray dynModes   { "Above", "Below", "Two-way" };
    inline const juce::StringArray detectModes { "Self", "Relative", "Sidechain" };
    inline const juce::StringArray monitorModes { "Off", "Mid", "Side" };

    juce::StringArray filterTypeNames();
    juce::StringArray slopeNames();

    /** A pleasant default frequency for each of the 32 band slots. */
    float defaultFreqForBand (int bandIndex);

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::String formatFrequency (float hz);
    juce::String formatDb (float db);
}
