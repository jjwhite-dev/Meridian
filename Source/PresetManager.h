#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class QTipProcessor;

/** Factory presets (built in) and user presets (.qtpreset files in
    Documents/Q-TIP/Presets). A preset holds every parameter. */
class PresetManager
{
public:
    explicit PresetManager (QTipProcessor&);

    struct Entry
    {
        juce::String name;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
    };

    static juce::File folder();
    static constexpr const char* extension = ".qtpreset";

    /** Factory presets first, then user presets sorted by name. */
    std::vector<Entry> list() const;

    bool load (const Entry&);
    bool loadFile (const juce::File&);
    bool save (const juce::File&);
    bool writeState (const juce::File&) const;
    void loadInit();

    /** The EQ a new instance starts with ("Save as default"). */
    static juce::File defaultFile();
    bool saveAsDefault();
    void clearDefault();
    /** Loads the saved default if there is one, else Init. Called when the plugin is created. */
    void loadStartupDefault();

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

    QTipProcessor& proc;
    juce::uint32 clearUndoAt = 0;
    Entry current { "Init", true, 0, {} };
};
