#include "GlobalSettings.h"

GlobalSettings::GlobalSettings()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Meridian EQ";
    o.filenameSuffix = ".settings";
    o.folderName = "Meridian EQ";
    o.osxLibrarySubFolder = "Application Support";
    file = std::make_unique<juce::PropertiesFile> (o);

    refreshHz     = juce::jlimit (15, 144, file->getIntValue ("refreshHz", 60));
    fftOrder      = juce::jlimit (11, 14, file->getIntValue ("fftOrder", 13));
    brightness    = (float) file->getDoubleValue ("brightness", 1.0);
    dbRange       = (float) file->getDoubleValue ("dbRange", 18.0);
    gpu           = file->getBoolValue ("gpu", true);
    uiScale       = juce::jlimit (0.5f, 3.0f, (float) file->getDoubleValue ("uiScale", 1.0));
    logicalWidth  = file->getIntValue ("logicalWidth", 1240);
    logicalHeight = file->getIntValue ("logicalHeight", 800);
    paletteIndex  = file->getIntValue ("palette", 0);

    const auto& presets = theme::presets();
    palette = presets[(size_t) juce::jlimit (0, (int) presets.size() - 1, juce::jmax (0, paletteIndex))];
    if (paletteIndex < 0)
    {
        palette.name = "Custom";
        for (const auto& s : theme::slots())
        {
            const auto key = juce::String ("colour_") + s.key;
            if (file->containsKey (key))
                palette.*(s.member) = juce::Colour::fromString (file->getValue (key));
        }
        palette.specFill = (float) file->getDoubleValue ("specFill", palette.specFill);
    }
    theme::apply (palette);
}

GlobalSettings::~GlobalSettings()
{
    save();
}

void GlobalSettings::save()
{
    file->setValue ("refreshHz", refreshHz);
    file->setValue ("fftOrder", fftOrder);
    file->setValue ("brightness", brightness);
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

void GlobalSettings::changed()
{
    theme::apply (palette);
    save();
    sendChangeMessage();
}
