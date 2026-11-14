#include "Controls.h"

// ---------------------------------------------------------------- Knob
Knob::Knob (const juce::String& n, bool amber, bool bipolar) : name (n)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.getProperties().set ("amber", amber);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (220);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void Knob::attach (juce::RangedAudioParameter* p)
{
    attachment.reset();
    if (p != nullptr) attachment = std::make_unique<juce::SliderParameterAttachment> (*p, slider, nullptr);
    repaint();
}

void Knob::detach() { attachment.reset(); }

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (16);
    r.removeFromBottom (18);
    const int s = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (s, s));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (isEnabled() ? theme::muted : theme::faint);
    g.setFont (theme::font (11.0f));
    g.drawText (name, r.removeFromTop (16), juce::Justification::centred);
    g.setColour (isEnabled() ? theme::text : theme::faint);
    g.setFont (theme::mono (12.0f));
    const auto text = isEnabled() ? slider.getTextFromValue (slider.getValue()) : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94"));
    g.drawText (text, r.removeFromBottom (18), juce::Justification::centred);
}

// ---------------------------------------------------------------- BarSlider
BarSlider::BarSlider (const juce::String& n, bool amber, bool bipolar) : name (n)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.getProperties().set ("amber", amber);
    slider.getProperties().set ("bipolar", bipolar);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void BarSlider::attach (juce::RangedAudioParameter* p)
{
    attachment.reset();
    if (p != nullptr) attachment = std::make_unique<juce::SliderParameterAttachment> (*p, slider, nullptr);
    repaint();
}

void BarSlider::detach() { attachment.reset(); }

void BarSlider::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (16));
}

void BarSlider::paint (juce::Graphics& g)
{
    auto top = getLocalBounds().removeFromTop (16);
    g.setFont (theme::font (11.0f));
    g.setColour (isEnabled() ? theme::muted : theme::faint);
    g.drawText (name, top, juce::Justification::centredLeft);
    g.setFont (theme::mono (12.0f));
    g.setColour (isEnabled() ? theme::text : theme::faint);
    g.drawText (slider.getTextFromValue (slider.getValue()), top, juce::Justification::centredRight);
}

// ---------------------------------------------------------------- SegmentedControl
SegmentedControl::SegmentedControl (juce::StringArray o, bool a) : options (std::move (o)), amber (a)
{
    for (int i = 0; i < options.size(); ++i) enabledFlags.add (true);
}

void SegmentedControl::attach (juce::RangedAudioParameter* p)
{
    attachment.reset();
    if (p == nullptr) return;
    attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v)
    {
        selected = juce::jlimit (0, options.size() - 1, juce::roundToInt (v));
        repaint();
    }, nullptr);
    attachment->sendInitialUpdate();
}

void SegmentedControl::detach() { attachment.reset(); }

void SegmentedControl::setSelected (int index, bool notify)
{
    selected = juce::jlimit (0, options.size() - 1, index);
    repaint();
    if (notify && onChange) onChange (selected);
}

void SegmentedControl::setOptionEnabled (int index, bool e)
{
    if (juce::isPositiveAndBelow (index, enabledFlags.size())) enabledFlags.set (index, e);
    repaint();
}

juce::Rectangle<float> SegmentedControl::segment (int i) const
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    float total = 0;
    juce::Array<float> widths;
    const auto f = theme::font (12.0f, true);
    for (auto& s : options)
    {
        const float w = juce::GlyphArrangement::getStringWidth (f, s) + 20.0f;
        widths.add (w);
        total += w;
    }
    // distribute any spare space evenly
    const float spare = juce::jmax (0.0f, r.getWidth() - total - 2.0f * (float) (options.size() - 1));
    float x = r.getX();
    for (int k = 0; k < i; ++k) x += widths[k] + spare / (float) options.size() + 2.0f;
    return { x, r.getY(), widths[i] + spare / (float) options.size(), r.getHeight() };
}

int SegmentedControl::indexAt (juce::Point<int> p) const
{
    for (int i = 0; i < options.size(); ++i)
        if (segment (i).contains (p.toFloat())) return i;
    return -1;
}

void SegmentedControl::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::display);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    const auto on = amber ? theme::amber : theme::accent;
    g.setFont (theme::font (12.0f, true));
    for (int i = 0; i < options.size(); ++i)
    {
        const auto s = segment (i);
        const bool en = isEnabled() && enabledFlags[i];
        if (i == selected && en)
        {
            g.setColour (juce::isPositiveAndBelow (i, optionColours.size()) ? optionColours[i] : on);
            g.fillRoundedRectangle (s, 6.0f);
        }
        else if (i == hover && en)
        {
            g.setColour (theme::well);
            g.fillRoundedRectangle (s, 6.0f);
        }
        const auto fillCol = juce::isPositiveAndBelow (i, optionColours.size()) ? optionColours[i] : on;
        g.setColour (! en ? theme::faint : (i == selected ? theme::on (fillCol) : theme::textDim));
        g.drawText (options[i], s, juce::Justification::centred);
    }
}

void SegmentedControl::mouseDown (const juce::MouseEvent& e)
{
    const int i = indexAt (e.getPosition());
    if (i < 0 || ! enabledFlags[i] || ! isEnabled()) return;
    if (attachment != nullptr) attachment->setValueAsCompleteGesture ((float) i);
    setSelected (i, true);
}

void SegmentedControl::mouseMove (const juce::MouseEvent& e)
{
    const int i = indexAt (e.getPosition());
    if (i != hover) { hover = i; repaint(); }
}

void SegmentedControl::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }

// ---------------------------------------------------------------- IconButton
IconButton::IconButton (const juce::String& n, juce::Path p) : juce::Button (n), icon (std::move (p))
{
    setTooltip (n);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    g.setColour (on ? theme::accent : (down ? theme::well : juce::Colours::transparentBlack));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (on ? theme::accent : (highlighted ? theme::border2 : theme::border));
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    const auto area = r.reduced (r.getWidth() * 0.28f);
    g.setColour (on ? theme::on (theme::accent) : theme::textDim);
    g.strokePath (icon, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  icon.getTransformToScaleToFit (area, true));
}

namespace icons
{
    juce::Path cross()
    {
        juce::Path p;
        p.startNewSubPath (0, 0); p.lineTo (10, 10);
        p.startNewSubPath (10, 0); p.lineTo (0, 10);
        return p;
    }
    juce::Path power()
    {
        juce::Path p;
        p.startNewSubPath (5, 0); p.lineTo (5, 4.5f);
        p.addCentredArc (5, 6, 4.5f, 4.5f, 0, 0.75f, juce::MathConstants<float>::twoPi - 0.75f, true);
        return p;
    }
    juce::Path gear()
    {
        juce::Path p;
        p.addEllipse (3.5f, 3.5f, 5.0f, 5.0f);
        for (int i = 0; i < 8; ++i)
        {
            const float a = (float) i * juce::MathConstants<float>::pi / 4.0f;
            p.startNewSubPath (6 + 4.0f * std::cos (a), 6 + 4.0f * std::sin (a));
            p.lineTo (6 + 6.0f * std::cos (a), 6 + 6.0f * std::sin (a));
        }
        return p;
    }
    juce::Path chevronLeft()
    {
        juce::Path p;
        p.startNewSubPath (6, 0); p.lineTo (1, 5); p.lineTo (6, 10);
        return p;
    }
    juce::Path chevronRight()
    {
        juce::Path p;
        p.startNewSubPath (1, 0); p.lineTo (6, 5); p.lineTo (1, 10);
        return p;
    }
    juce::Path save()
    {
        juce::Path p;
        p.startNewSubPath (0, 0); p.lineTo (8.5f, 0); p.lineTo (11, 2.5f); p.lineTo (11, 11); p.lineTo (0, 11); p.closeSubPath();
        p.startNewSubPath (3, 0); p.lineTo (3, 3.5f); p.lineTo (8, 3.5f); p.lineTo (8, 0);
        p.addRectangle (2.5f, 6.5f, 6.0f, 4.5f);
        return p;
    }
    juce::Path undo()
    {
        juce::Path p;
        p.startNewSubPath (3, 1); p.lineTo (0.5f, 3.5f); p.lineTo (3, 6);
        p.startNewSubPath (0.5f, 3.5f); p.lineTo (7, 3.5f);
        p.cubicTo (10.5f, 3.5f, 10.5f, 10, 7, 10); p.lineTo (4, 10);
        return p;
    }
    juce::Path redo()
    {
        juce::Path p = undo();
        p.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (11.0f, 0.0f));
        return p;
    }
    juce::Path speaker()
    {
        juce::Path p;
        p.startNewSubPath (0, 3.5f); p.lineTo (2.5f, 3.5f); p.lineTo (6, 0.5f); p.lineTo (6, 11.5f); p.lineTo (2.5f, 8.5f);
        p.lineTo (0, 8.5f); p.closeSubPath();
        p.startNewSubPath (8.5f, 3.5f);
        p.quadraticTo (10.5f, 6, 8.5f, 8.5f);
        return p;
    }
}
