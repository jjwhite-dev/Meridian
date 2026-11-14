#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class MeridianProcessor;

/** Factory presets (built in) and user presets (.mepreset files in
    Documents/Meridian EQ/Presets). A preset holds every parameter. */
class PresetManager
{
public:
    explicit PresetManager (MeridianProcessor&);

    struct Entry
    {
        juce::String name;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
    };

    static juce::File folder();
    static constexpr const char* extension = ".mepreset";

    /** Factory presets first, then user presets sorted by name. */
    std::vector<Entry> list() const;

    bool load (const Entry&);
    bool loadFile (const juce::File&);
    bool save (const juce::File&);
    void loadInit();

    /** Steps through list() (wraps around). */
    void step (int delta);

    /** Called regularly by the editor: clears undo history shortly after a preset
        load, once the parameter changes have reached the state tree. */
    void tick();

    juce::String currentName() const { return current.name; }
    /** Restores the name shown after a session reload. */
    void setCurrentName (const juce::String& name);
    const Entry& currentEntry() const { return current; }

private:
    void applyFactory (int index);

    void scheduleUndoClear();

    MeridianProcessor& proc;
    juce::uint32 clearUndoAt = 0;
    Entry current { "Init", true, 0, {} };
};
