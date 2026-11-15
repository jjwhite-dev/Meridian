#include "GlobalSettings.h"

GlobalSettings::GlobalSettings()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Meridian EQ";
    o.filenameSuffix = ".settings";
    o.folderName = "Meridian EQ";
    o.osxLibrarySubFolder = "Application Support";
    file = std::make_unique<juce::PropertiesFile> (o);

    setFactory();
    loadFrom (*file);
    theme::apply (palette);
}

void GlobalSettings::setFactory()
{
    refreshHz = 60;
    fftOrder = 13;
    brightness = 1.0f;
    spectrumStyle = 0;
    particleDot = 2.4f;
    particleSpacing = 6;
    spectrumGrab = false;
    dbRange = 18.0f;
    gpu = true;
    uiScale = 1.0f;
    logicalWidth = 1240;
    logicalHeight = 800;
    paletteIndex = 0;
    palette = theme::presets().front();
}

void GlobalSettings::loadFrom (juce::PropertiesFile& f)
{
    refreshHz       = juce::jlimit (15, 144, f.getIntValue ("refreshHz", refreshHz));
    fftOrder        = juce::jlimit (11, 14, f.getIntValue ("fftOrder", fftOrder));
    brightness      = (float) f.getDoubleValue ("brightness", brightness);
    spectrumStyle   = juce::jlimit (0, 1, f.getIntValue ("spectrumStyle", spectrumStyle));
    particleDot     = juce::jlimit (1.0f, 6.0f, (float) f.getDoubleValue ("particleDot", particleDot));
    particleSpacing = juce::jlimit (3, 14, f.getIntValue ("particleSpacing", particleSpacing));
    spectrumGrab    = f.getBoolValue ("spectrumGrab", spectrumGrab);
    dbRange         = (float) f.getDoubleValue ("dbRange", dbRange);
    gpu             = f.getBoolValue ("gpu", gpu);
    uiScale         = juce::jlimit (0.5f, 3.0f, (float) f.getDoubleValue ("uiScale", uiScale));
    logicalWidth    = f.getIntValue ("logicalWidth", logicalWidth);
    logicalHeight   = f.getIntValue ("logicalHeight", logicalHeight);
    paletteIndex    = f.getIntValue ("palette", paletteIndex);

    const auto& presets = theme::presets();
    palette = presets[(size_t) juce::jlimit (0, (int) presets.size() - 1, juce::jmax (0, paletteIndex))];
    if (paletteIndex < 0)
    {
        palette.name = "Custom";
        for (const auto& s : theme::slots())
        {
            const auto key = juce::String ("colour_") + s.key;
            if (f.containsKey (key))
                palette.*(s.member) = juce::Colour::fromString (f.getValue (key));
        }
        palette.specFill = (float) f.getDoubleValue ("specFill", palette.specFill);
    }
}

void GlobalSettings::saveAsDefaults()
{
    file->clear();
    file->setValue ("refreshHz", refreshHz);
    file->setValue ("fftOrder", fftOrder);
    file->setValue ("brightness", brightness);
    file->setValue ("spectrumStyle", spectrumStyle);
    file->setValue ("particleDot", particleDot);
    file->setValue ("particleSpacing", particleSpacing);
    file->setValue ("spectrumGrab", spectrumGrab);
    file->setValue ("dbRange", dbRange);
    file->setValue ("gpu", gpu);
    file->setValue ("uiScale", uiScale);
    file->setValue ("logicalWidth", logicalWidth);
    file->setValue ("logicalHeight", logicalHeight);
    file->setValue ("palette", paletteIndex);
    for (const auto& s : theme::slots())
        file->setValue (juce::String ("colour_") + s.key, (palette.*(s.member)).toString());
    file->setValue ("specFill", palette.specFill);
    file->saveIfNeeded();
}

void GlobalSettings::resetToFactory()
{
    file->clear();
    file->saveIfNeeded();
    setFactory();
    changed();
}

void GlobalSettings::changed()
{
    theme::apply (palette);
    sendChangeMessage();
}
