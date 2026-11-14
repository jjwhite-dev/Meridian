#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>

#include "PluginProcessor.h"
#include "ui/EqDisplay.h"
#include "ui/LookAndFeel.h"
#include "ui/Panels.h"

class MeridianEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MeridianEditor (MeridianProcessor&);
    ~MeridianEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectBand (int band);
    void showSettings();
    void applySettings();

    MeridianProcessor& proc;
    MeridianLookAndFeel lnf;
    juce::OpenGLContext glContext;
    juce::TooltipWindow tooltips { this, 700 };

    HeaderBar header;
    EqDisplay display;
    BandStrip strip;
    BandPanel bandPanel;
    DynamicsPanel dynamicsPanel;
    GlobalPanel globalPanel;

    int selected = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeridianEditor)
};
