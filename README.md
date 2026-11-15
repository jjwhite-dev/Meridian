# Meridian EQ — prototype 0.2.1

A 32-band parametric / dynamic EQ plugin (VST3 on Windows and macOS, AU on
macOS) with a GPU-drawn interface. This is the first working prototype: the
core EQ, dynamics, sidechain, routing and display are in; several of the
"precision" features from the spec are still to come (see the status table
below).

## Install

1. Open the **Actions** tab on GitHub, pick the latest successful
   **Build** run and download `Meridian-EQ-VST3-Windows` or
   `Meridian-EQ-macOS`.
2. Windows: copy `Meridian EQ.vst3` to `C:\Program Files\Common Files\VST3\`.
   macOS: copy `Meridian EQ.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and
   `Meridian EQ.component` to `~/Library/Audio/Plug-Ins/Components/`. The
   builds are not signed, so run
   `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/*/Meridian\ EQ.*`
   once.
3. Rescan plugins in your DAW. It shows up as an effect called **Meridian EQ**.

## Using it

- **Presets**: the preset field in the header. Arrows step through all
  presets; click the name for the list (7 factory presets plus yours), Save,
  Save as, Load from file, Show presets folder and Init. Your presets are
  `.mepreset` files in `Documents/Meridian EQ/Presets`.
- **Undo / redo**: the arrows left of the preset field, or Ctrl/Cmd+Z and
  Ctrl/Cmd+Shift+Z (Ctrl+Y). Every click starts a new step, so a whole drag
  undoes at once. Loading a preset clears the history.
- **Select / deselect**: click a dot or a band chip to select it; click
  empty space in the display, the band strip, the header, the Global panel or
  between panels (or press Esc) to deselect.

- **Add a band**: double-click the display (a bell at that spot), click
  **+ Add band**, or *Spectrum Grab*: move the mouse near a peak in the
  analyzer until a dashed ring appears, then click and drag. That creates a
  narrow bell on the peak and keeps dragging it.
- **Edit**: drag a dot (left/right = frequency, up/down = gain; hold Shift to
  change only the gain), mouse wheel over a dot for Q, double-click a dot to
  delete it, right-click for type, channel, dynamics, bypass and solo.
- **Audition**: the speaker button above a dot (shown on the selected and the
  hovered band) switches audition on and off. While it is on you hear only
  that band's range (cut bands play what they remove), everything outside the
  range is dimmed and a banner shows what you hear. **S** in the band panel
  and the right-click menu do the same. The analyzer always shows the
  processed signal, so moving a band shows up in the spectrum straight away.
- **Colours**: every processing channel has its own colour — Stereo cyan,
  Left blue, Right pink, Mid green, Side violet. Each band's own curve is
  drawn in its channel's colour on top of the analyzer, together with the
  combined curve: white for stereo, and one curve per channel (stereo bands
  included) when L / R / M / S bands are in use.
- **Analyzer**: 1/6-octave smoothed on a log frequency axis and drawn as
  smooth curves (no corners), with fast rise and slower fall.
- **Hover** anywhere for frequency, analyzer level and EQ gain at that point.
  Click the dB scale on the right to switch between ±6, ±12, ±18 and ±30 dB.
- **Per band**: 15 filter types, each shown with an icon of its curve (in the
  type picker, the right-click menu and the band strip), slope 6–96 dB/oct for
  the cut filters, Stereo / L / R / Mid / Side processing.
- **Dynamics** (any band type that has gain): threshold, range (the most the
  band moves, negative = cut, positive = boost), ratio, attack, release.
  - *Above / Below / Two-way*: react over the threshold, under it, or both
    (in opposite directions).
  - *Onset / Body*: extra movement on the transient part vs. the sustained
    part of the detected signal (negative = less, positive = more).
  - *Detection*: **Self** (the band's own range), **Relative** (another range
    of the same signal: "Listen at" / "Width"), **Sidechain** (the plugin's
    sidechain input, filtered to that range). The display shows the band's
    possible dynamic range in amber and the live curve in white; each
    dynamic dot has an amber stem that moves with the gain.
- **Global**: gain scale (−200…200 %), stereo width, output, lookahead
  (dynamics see the signal before it is processed), auto gain, M/S monitor,
  2× oversampling, phase mode.
- **Options** (gear, top right): a panel slides in from the right with
  - Display: spectrum style — **Smooth** (filled curve) or **Particles**
    (columns of glowing dots in the spectrum's low colour, sparks in its high
    colour that get denser towards the highs, and peak dots that hang and
    fall; the EQ curve gets a soft glow), with dot size and dot spacing;
    spectrum refresh rate (default 60 fps), resolution (default High),
    brightness, EQ scale.
  - Interface: size 80 / 100 / 125 / 150 / 175 / 200 %, and the renderer:
    **GPU (OpenGL)** (default) or **CPU (software)**. The status line in the
    Global panel shows which one is drawing.
  - Colour presets: Graphite, Midnight, Nordic Frost, Obsidian Gold, Rose
    Quartz, Forest, Neon Noir, Solar Ember, Nexus (black, magenta and orange,
    made for the Particles style), Sandstone and Arctic (the last two are
    light themes).
  - Custom colours: click any swatch (background, panels, display, text,
    accent, dynamics, each channel colour, and the two ends of the spectrum
    gradient) to pick a colour; spectrum fill opacity; reset to the preset.
  These options are saved for all instances (in the plugin's settings file,
  not in the session), like the window size.

### Sidechain in your DAW

The plugin has a second (stereo) input bus called *Sidechain*. In Ableton
Live use the sidechain section of the device, in Cubase activate the side
chain button, in Reaper give the track 4 channels and send to channels 3/4,
in Logic choose the side chain source in the plugin header. Then set a
band's detection to **Sidechain**.

## Status against the spec

| Feature | In this build |
| --- | --- |
| 32 bands | Yes |
| 15 filter types | Yes: Bell, Low/High Shelf, Low/High Cut, Notch, Band Pass, Tilt Shelf, Flat Tilt, Flat Top, Sword, Brickwall Low/High, Allpass, Proportional Bell |
| Slopes 0–96 dB/oct | 6–96 dB/oct in 6 dB steps (continuous slopes later) |
| Vintage emulations (32 types / 9 units) | Not yet |
| 64-bit processing | Yes, everything runs in double precision |
| 117-bit "double-double" | Not yet |
| Nyquist-matched transform | Yes ("Analog" phase mode): within ~1 dB of the analog curve at 15 kHz / 48 kHz where the classic transform is off by up to 9.5 dB |
| Adaptive filter topologies | Not yet (one topology, transposed direct form II in double) |
| Phase modes | Zero Latency (classic bilinear) and Analog (matched). Linear and Mixed are shown but not yet available |
| Dynamic EQ, two-way thresholds, relative detection, onset/body | Yes |
| External sidechain | Yes |
| Per-band L / R / M / S, width, M/S monitor | Yes |
| Spectrum Grab, audition, hover readout | Yes |
| OpenGL GUI, themes, refresh rate, resolution, brightness | Yes: GPU or CPU renderer, 10 colour presets plus custom colours and spectrum gradient, interface sizes |
| Presets, undo / redo | Yes |
| 2× oversampling, lookahead, auto gain, gain scale | Yes |
| Hand-written assembly | No (plain C++; about 3 % of one core for 32 bands with 16 dynamic at 48 kHz) |

## Building

Needs CMake 3.22+ and a C++17 compiler. JUCE 8.0.10 is downloaded
automatically (or pass `-DMERIDIAN_JUCE_DIR=/path/to/JUCE`).

```
cmake -S . -B build
cmake --build build --config Release --target MeridianEQ_VST3
```

The DSP has unit tests that need no JUCE:

```
cmake -S . -B build-tests -DMERIDIAN_TESTS_ONLY=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## Code layout

- `Source/dsp/FilterDesign.h` – filter shapes as analog sections, and the two
  transforms (Nyquist-matched after Vicanek 2016, and bilinear). No JUCE.
- `Source/dsp/EqEngine.*` – the whole audio path: bands, channel modes,
  dynamics, sidechain, lookahead, audition, width, monitor, auto gain. No JUCE.
- `Source/PluginProcessor.*`, `Source/Params.*` – JUCE wrapper, parameters
  (32 × 20 band parameters + 8 global), oversampling, analyzer queues.
- `Source/PluginEditor.*`, `Source/ui/*` – the interface.
- `tests/` – filter design and engine tests.
