#include "PresetManager.h"
#include "PluginProcessor.h"

namespace pb = params::band;

namespace
{
    struct FactoryPreset { const char* name; };
    const FactoryPreset factory[] = {
        { "Init" },
        { "Vocal Presence" },
        { "Kick Ducks Bass (sidechain)" },
        { "Master Air & Tight Lows" },
        { "Dynamic De-Esser" },
        { "Resonance Tamer" },
        { "Mid/Side Widener" },
    };
    constexpr int numFactory = (int) (sizeof (factory) / sizeof (factory[0]));
}

PresetManager::PresetManager (MeridianProcessor& p) : proc (p) {}

juce::File PresetManager::folder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("Meridian EQ").getChildFile ("Presets");
    f.createDirectory();
    return f;
}

std::vector<PresetManager::Entry> PresetManager::list() const
{
    std::vector<Entry> out;
    for (int i = 0; i < numFactory; ++i) out.push_back ({ factory[i].name, true, i, {} });

    auto files = folder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0;
    });
    for (auto& f : files) out.push_back ({ f.getFileNameWithoutExtension(), false, -1, f });
    return out;
}

bool PresetManager::load (const Entry& e)
{
    if (e.factory)
    {
        applyFactory (e.factoryIndex);
        current = e;
        scheduleUndoClear();
        return true;
    }
    return loadFile (e.file);
}

bool PresetManager::loadFile (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("MeridianEQPreset")) return false;
    auto* stateXml = xml->getChildByName (proc.apvts.state.getType());
    if (stateXml == nullptr) return false;

    proc.apvts.replaceState (juce::ValueTree::fromXml (*stateXml));
    scheduleUndoClear();
    current = { file.getFileNameWithoutExtension(), false, -1, file };
    return true;
}

bool PresetManager::save (const juce::File& target)
{
    auto file = target.withFileExtension (extension);
    juce::XmlElement root ("MeridianEQPreset");
    root.setAttribute ("version", 1);
    if (auto stateXml = proc.apvts.copyState().createXml())
        root.addChildElement (stateXml.release());
    if (! root.writeTo (file)) return false;
    current = { file.getFileNameWithoutExtension(), false, -1, file };
    return true;
}

void PresetManager::scheduleUndoClear()
{
    proc.undoManager.clearUndoHistory();
    clearUndoAt = juce::Time::getMillisecondCounter() + 400;
}

void PresetManager::tick()
{
    if (clearUndoAt != 0 && juce::Time::getMillisecondCounter() >= clearUndoAt)
    {
        proc.undoManager.clearUndoHistory();
        clearUndoAt = 0;
    }
}

void PresetManager::setCurrentName (const juce::String& name)
{
    for (const auto& e : list())
        if (e.name == name) { current = e; return; }
    current = { name, false, -1, {} };
}

void PresetManager::loadInit()
{
    load ({ factory[0].name, true, 0, {} });
}

void PresetManager::step (int delta)
{
    const auto all = list();
    if (all.empty()) return;
    int idx = 0;
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& e = all[(size_t) i];
        if (e.factory == current.factory && (e.factory ? e.factoryIndex == current.factoryIndex : e.file == current.file)) { idx = i; break; }
    }
    idx = (idx + delta + (int) all.size()) % (int) all.size();
    load (all[(size_t) idx]);
}

void PresetManager::applyFactory (int index)
{
    // start from defaults for every parameter
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
    proc.lockedSolo = -1;
    proc.auditionBand = -1;

    using FT = meq::FilterType;
    auto band = [this] (FT t, float f, float g, float q) { return proc.addBand (t, f, g, q); };
    auto set = [this] (int b, const char* s, float v) { if (b >= 0) proc.setBandValue (b, s, v); };
    auto dyn = [&] (int b, float thr, float range, float ratio, float att, float rel)
    {
        set (b, pb::dyn, 1); set (b, pb::thr, thr); set (b, pb::range, range);
        set (b, pb::ratio, ratio); set (b, pb::attack, att); set (b, pb::release, rel);
    };

    switch (index)
    {
        case 1: // Vocal Presence
        {
            int b = band (FT::LowCut, 85, 0, 0.707f); set (b, pb::slope, 3); // 24 dB/oct
            b = band (FT::Bell, 300, -2.5f, 1.2f); set (b, pb::channel, 3);
            band (FT::Bell, 3200, 2.0f, 0.9f);
            b = band (FT::Bell, 6500, 0, 3.0f); dyn (b, -30, -5, 3, 1, 60);
            band (FT::HighShelf, 11000, 2.5f, 0.7f);
            break;
        }
        case 2: // Kick ducks bass
        {
            int b = band (FT::Bell, 70, 0, 0.9f);
            dyn (b, -32, -7, 4, 2, 90);
            set (b, pb::detect, 2); set (b, pb::detFreq, 60); set (b, pb::detQ, 1.0f);
            break;
        }
        case 3: // Master air & tight lows
        {
            int b = band (FT::LowCut, 25, 0, 0.707f); set (b, pb::slope, 2); // 18 dB/oct
            band (FT::LowShelf, 90, 1.0f, 0.7f);
            band (FT::Bell, 250, -1.0f, 0.7f);
            band (FT::HighShelf, 12000, 2.0f, 0.6f);
            break;
        }
        case 4: // De-esser
        {
            int b = band (FT::Bell, 6800, 0, 2.5f);
            dyn (b, -32, -9, 5, 0.5f, 70);
            set (b, pb::onset, -30);
            break;
        }
        case 5: // Resonance tamer
        {
            int b = band (FT::Sword, 2500, -3.0f, 4.0f);
            dyn (b, -28, -6, 3, 3, 120);
            b = band (FT::Sword, 4800, -2.0f, 5.0f);
            dyn (b, -30, -5, 3, 3, 120);
            break;
        }
        case 6: // M/S widener
        {
            int b = band (FT::LowCut, 140, 0, 0.707f); set (b, pb::channel, 4); set (b, pb::slope, 1);
            b = band (FT::HighShelf, 7000, 2.5f, 0.7f); set (b, pb::channel, 4);
            b = band (FT::Bell, 2500, -1.0f, 0.8f); set (b, pb::channel, 3);
            proc.apvts.getParameter (params::width)->setValueNotifyingHost (proc.apvts.getParameter (params::width)->convertTo0to1 (115.0f));
            break;
        }
        default: break; // Init
    }
}
