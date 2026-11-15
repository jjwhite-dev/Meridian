#include "LookAndFeel.h"
#include "Theme.h"

QTipLookAndFeel::QTipLookAndFeel()
{
    applyTheme();
}

void QTipLookAndFeel::applyTheme()
{
    setColour (juce::ResizableWindow::backgroundColourId, theme::bg);
    setColour (juce::ComboBox::backgroundColourId, theme::well);
    setColour (juce::ComboBox::outlineColourId, theme::border2);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::arrowColourId, theme::muted);
    setColour (juce::PopupMenu::backgroundColourId, theme::well);
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accent.withAlpha (0.2f));
    setColour (juce::PopupMenu::highlightedTextColourId, theme::text);
    setColour (juce::PopupMenu::headerTextColourId, theme::muted);
    setColour (juce::TextButton::buttonColourId, theme::panel);
    setColour (juce::TextButton::buttonOnColourId, theme::accent);
    setColour (juce::TextButton::textColourOffId, theme::textDim);
    setColour (juce::TextButton::textColourOnId, theme::onAccent);
    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::TooltipWindow::backgroundColourId, theme::text);
    setColour (juce::TooltipWindow::textColourId, theme::bg);
    setColour (juce::TooltipWindow::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void QTipLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                            float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const float rArc = size * 0.5f - 3.0f;
    const float rCap = rArc * 0.68f;
    const bool enabled = s.isEnabled();
    const auto valueColour = (bool) s.getProperties()["amber"] ? theme::amber : theme::accent;

    g.setColour (theme::well);
    g.fillEllipse (c.x - rCap, c.y - rCap, rCap * 2, rCap * 2);
    g.setColour (theme::border2);
    g.drawEllipse (c.x - rCap, c.y - rCap, rCap * 2, rCap * 2, 1.0f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, startAngle, endAngle, true);
    g.setColour (theme::border);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float valueAngle = startAngle + pos * (endAngle - startAngle);
    const float fromAngle = (bool) s.getProperties()["bipolar"] ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (valueAngle - fromAngle) > 0.01f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, juce::jmin (fromAngle, valueAngle), juce::jmax (fromAngle, valueAngle), true);
        g.setColour (enabled ? valueColour : theme::faint);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto dir = juce::Point<float> (std::sin (valueAngle), -std::cos (valueAngle));
    g.setColour (enabled ? theme::text : theme::faint);
    g.drawLine ({ c + dir * (rCap * 0.3f), c + dir * (rCap * 0.85f) }, 2.0f);
}

void QTipLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                            juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    const auto r = juce::Rectangle<float> ((float) x, (float) y + h * 0.5f - 3.0f, (float) w, 6.0f);
    g.setColour (theme::border);
    g.fillRoundedRectangle (r, 3.0f);

    const bool bipolar = (bool) s.getProperties()["bipolar"];
    const float centre = r.getX() + r.getWidth() * 0.5f;
    const float from = bipolar ? centre : r.getX();
    const float to = pos;
    auto fill = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (from, to), r.getY(), juce::jmax (from, to), r.getBottom());
    g.setColour (! s.isEnabled() ? theme::faint : (bool) s.getProperties()["amber"] ? theme::amber : theme::accent);
    g.fillRoundedRectangle (fill, 3.0f);

    if (bipolar)
    {
        g.setColour (theme::faint);
        g.fillRect (centre - 0.5f, r.getY() - 2.0f, 1.0f, r.getHeight() + 4.0f);
    }
    g.setColour (s.isEnabled() ? theme::text : theme::faint);
    g.fillEllipse (pos - 6.0f, r.getCentreY() - 6.0f, 12.0f, 12.0f);
}

void QTipLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    if ((bool) b.getProperties()["switch"])
    {
        const auto sw = juce::Rectangle<float> (36.0f, 20.0f).withCentre ({ r.getRight() - 18.0f, r.getCentreY() });
        const auto onColour = (bool) b.getProperties()["amber"] ? theme::amber : theme::accent;
        g.setColour (on ? onColour : theme::faint);
        g.fillRoundedRectangle (sw, 10.0f);
        const float knobX = on ? sw.getRight() - 18.0f : sw.getX() + 2.0f;
        g.setColour (theme::bg);
        g.fillEllipse (knobX, sw.getY() + 2.0f, 16.0f, 16.0f);
        if (b.getButtonText().isNotEmpty())
        {
            g.setColour (theme::muted);
            g.setFont (theme::font (12.0f));
            g.drawText (b.getButtonText(), r.withTrimmedRight (44.0f), juce::Justification::centredLeft);
        }
        return;
    }

    // pill toggle with a status dot
    g.setColour (on ? theme::banner : theme::panel);
    g.fillRoundedRectangle (r.reduced (0.5f), 8.0f);
    g.setColour (on ? theme::accent.withAlpha (0.45f) : (highlighted ? theme::border2 : theme::border));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (on ? theme::accent : theme::faint);
    g.fillEllipse (12.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.setColour (on ? theme::text : theme::muted);
    g.setFont (theme::font (12.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (26.0f).withTrimmedRight (8.0f), juce::Justification::centredLeft);
}

void QTipLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? theme::accent : (down ? theme::well : theme::panel));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (on ? theme::accent : (highlighted ? theme::border2 : theme::border));
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
}

void QTipLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (theme::font (12.0f, true));
    g.setColour (b.getToggleState() ? theme::onAccent : (b.isEnabled() ? theme::textDim : theme::faint));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

juce::Font QTipLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return theme::font (12.0f, true); }

void QTipLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (theme::well);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (box.hasKeyboardFocus (true) ? theme::accent.withAlpha (0.6f) : theme::border2);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.0f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (theme::muted);
    g.strokePath (arrow, juce::PathStrokeType (1.5f));
}

juce::Font QTipLookAndFeel::getComboBoxFont (juce::ComboBox&) { return theme::font (13.0f, true); }
juce::Font QTipLookAndFeel::getPopupMenuFont() { return theme::font (13.0f); }

void QTipLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void QTipLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (theme::well);
    g.setColour (theme::border2);
    g.drawRect (0, 0, w, h);
}
