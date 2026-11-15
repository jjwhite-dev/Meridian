#include "Params.h"

namespace params
{

juce::StringArray filterTypeNames()
{
    juce::StringArray s;
    for (int i = 0; i < (int) qtip::FilterType::Count; ++i)
        s.add (qtip::filterTypeName ((qtip::FilterType) i));
    return s;
}

juce::StringArray slopeNames()
{
    juce::StringArray s;
    for (int d = 6; d <= 96; d += 6) s.add (juce::String (d) + " dB/oct");
    return s;
}

float defaultFreqForBand (int bandIndex)
{
    return (float) (40.0 * std::pow (400.0, bandIndex / 31.0));
}

juce::String formatFrequency (float hz)
{
    if (hz >= 10000.0f) return juce::String (hz / 1000.0f, 1) + " kHz";
    if (hz >= 1000.0f)  return juce::String (hz / 1000.0f, 2) + " kHz";
    if (hz >= 100.0f)   return juce::String (juce::roundToInt (hz)) + " Hz";
    return juce::String (hz, 1) + " Hz";
}

juce::String formatDb (float db)
{
    const float r = std::round (db * 10.0f) / 10.0f;
    return (r > 0.0f ? "+" : "") + juce::String (r, 1) + " dB";
}

namespace
{
    juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        return { lo, hi,
                 [] (float s, float e, float v) { return s * std::pow (e / s, v); },
                 [] (float s, float e, float x) { return std::log (x / s) / std::log (e / s); },
                 [] (float s, float e, float x) { return juce::jlimit (s, e, x); } };
    }

    using Float = juce::AudioParameterFloat;
    using Attr  = juce::AudioParameterFloatAttributes;

    std::unique_ptr<Float> makeFloat (const juce::String& id, const juce::String& name, juce::NormalisableRange<float> r,
                                      float def, std::function<juce::String (float, int)> text, const juce::String& label = {})
    {
        return std::make_unique<Float> (juce::ParameterID { id, 1 }, name, r, def,
                                        Attr().withStringFromValueFunction (std::move (text)).withLabel (label));
    }

    juce::String pct (float v, int) { return juce::String (juce::roundToInt (v)) + "%"; }
    juce::String ms (float v, int)  { return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms"; }
    juce::String hz (float v, int)  { return formatFrequency (v); }
    juce::String db (float v, int)  { return formatDb (v); }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int b = 0; b < qtip::kNumBands; ++b)
    {
        const juce::String n = "Band " + juce::String (b + 1) + " ";
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("band" + juce::String (b + 1), "Band " + juce::String (b + 1), "|");

        group->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { bandId (b, band::on), 1 }, n + "On", false));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { bandId (b, band::type), 1 }, n + "Type", filterTypeNames(), 0));
        group->addChild (makeFloat (bandId (b, band::freq), n + "Frequency", logRange (10.0f, 22000.0f), defaultFreqForBand (b), hz, "Hz"));
        group->addChild (makeFloat (bandId (b, band::gain), n + "Gain", { -30.0f, 30.0f, 0.01f }, 0.0f, db, "dB"));
        group->addChild (makeFloat (bandId (b, band::q), n + "Q", logRange (0.025f, 40.0f), 0.707f,
                                    [] (float v, int) { return juce::String (v, v < 10.0f ? 2 : 1); }));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { bandId (b, band::slope), 1 }, n + "Slope", slopeNames(), 1));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { bandId (b, band::channel), 1 }, n + "Channel",
                                                                       juce::StringArray { "Stereo", "Left", "Right", "Mid", "Side" }, 0));
        group->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { bandId (b, band::bypass), 1 }, n + "Bypass", false));
        group->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { bandId (b, band::dyn), 1 }, n + "Dynamic", false));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { bandId (b, band::dynMode), 1 }, n + "Dyn Mode", dynModes, 0));
        group->addChild (makeFloat (bandId (b, band::thr), n + "Threshold", { -80.0f, 0.0f, 0.1f }, -24.0f, db, "dB"));
        group->addChild (makeFloat (bandId (b, band::range), n + "Range", { -24.0f, 24.0f, 0.1f }, -6.0f, db, "dB"));
        group->addChild (makeFloat (bandId (b, band::ratio), n + "Ratio", logRange (1.0f, 20.0f), 2.0f,
                                    [] (float v, int) { return juce::String (v, 1) + ":1"; }));
        group->addChild (makeFloat (bandId (b, band::attack), n + "Attack", logRange (0.1f, 300.0f), 10.0f, ms, "ms"));
        group->addChild (makeFloat (bandId (b, band::release), n + "Release", logRange (5.0f, 3000.0f), 120.0f, ms, "ms"));
        group->addChild (makeFloat (bandId (b, band::onset), n + "Onset", { -100.0f, 100.0f, 1.0f }, 0.0f, pct, "%"));
        group->addChild (makeFloat (bandId (b, band::body), n + "Body", { -100.0f, 100.0f, 1.0f }, 0.0f, pct, "%"));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { bandId (b, band::detect), 1 }, n + "Detection",
                                                                       juce::StringArray { "Self", "Relative", "External Sidechain" }, 0));
        group->addChild (makeFloat (bandId (b, band::detFreq), n + "Detect Freq", logRange (20.0f, 20000.0f), 1000.0f, hz, "Hz"));
        group->addChild (makeFloat (bandId (b, band::detQ), n + "Detect Q", logRange (0.1f, 20.0f), 1.0f,
                                    [] (float v, int) { return juce::String (v, 2); }));
        layout.add (std::move (group));
    }

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { phase, 1 }, "Phase Mode", phaseModes, 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { oversample, 1 }, "2x Oversampling", false));
    layout.add (makeFloat (gainScale, "Gain Scale", { -200.0f, 200.0f, 1.0f }, 100.0f, pct, "%"));
    layout.add (makeFloat (output, "Output Gain", { -24.0f, 24.0f, 0.1f }, 0.0f, db, "dB"));
    layout.add (makeFloat (width, "Stereo Width", { 0.0f, 200.0f, 1.0f }, 100.0f, pct, "%"));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { autoGain, 1 }, "Auto Gain", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { monitor, 1 }, "M/S Monitor", monitorModes, 0));
    layout.add (makeFloat (lookahead, "Lookahead", { 0.0f, 20.0f, 0.1f }, 0.0f, ms, "ms"));

    return layout;
}

} // namespace params
