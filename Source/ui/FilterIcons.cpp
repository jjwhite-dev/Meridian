#include "FilterIcons.h"
#include "Theme.h"

namespace filtericons
{

namespace
{
    qtip::BandDesign typical (qtip::FilterType t)
    {
        using FT = qtip::FilterType;
        switch (t)
        {
            case FT::Bell:             return { t, 1000, 9, 1.2, 12 };
            case FT::ProportionalBell: return { t, 1000, 9, 0.7, 12 };
            case FT::LowShelf:         return { t, 250, 8, 0.8, 12 };
            case FT::HighShelf:        return { t, 4000, 8, 0.8, 12 };
            case FT::LowCut:           return { t, 250, 0, 0.707, 24 };
            case FT::HighCut:          return { t, 4000, 0, 0.707, 24 };
            case FT::Notch:            return { t, 1000, 0, 2.5, 12 };
            case FT::BandPass:         return { t, 1000, 0, 1.2, 12 };
            case FT::TiltShelf:        return { t, 1000, 12, 0.7, 12 };
            case FT::FlatTilt:         return { t, 1000, 14, 0.7, 12 };
            case FT::FlatTop:          return { t, 1000, 9, 0.9, 12 };
            case FT::Sword:            return { t, 1000, -13, 2.5, 12 };
            case FT::BrickwallLow:     return { t, 250, 0, 0.707, 96 };
            case FT::BrickwallHigh:    return { t, 4000, 0, 0.707, 96 };
            case FT::Allpass:          return { t, 1000, 0, 0.9, 12 };
            case FT::Count:            break;
        }
        return { FT::Bell, 1000, 0, 1, 12 };
    }
}

juce::Path curve (qtip::FilterType type, juce::Rectangle<float> area)
{
    constexpr double fs = 48000.0, range = 15.0;
    qtip::Cascade c;
    qtip::designBand (c, typical (type), fs, qtip::Transform::Matched);

    juce::Path p;
    const int n = 48;
    double unwrap = 0, prev = 0;
    for (int i = 0; i <= n; ++i)
    {
        const double f = 30.0 * std::pow (600.0, (double) i / n);
        const double w = 2.0 * qtip::kPi * f / fs;
        double v;
        if (type == qtip::FilterType::Allpass)
        {
            // allpass: draw its phase sweep (0 to -360 degrees) instead of a flat line
            const double ph = std::arg (c.response (w));
            double d = ph - prev;
            while (d > qtip::kPi) d -= 2 * qtip::kPi;
            while (d < -qtip::kPi) d += 2 * qtip::kPi;
            unwrap = (i == 0) ? ph : unwrap + d;
            prev = ph;
            v = range * (1.0 + unwrap / qtip::kPi) * 0.8; // 0..-2pi mapped to +range..-range
        }
        else
        {
            v = juce::jlimit (-range, range, c.magnitudeDb (w));
        }
        const float x = area.getX() + area.getWidth() * (float) i / (float) n;
        const float y = area.getCentreY() - (float) (v / range) * area.getHeight() * 0.5f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    return p;
}

std::unique_ptr<juce::Drawable> drawable (qtip::FilterType type, juce::Colour colour)
{
    auto d = std::make_unique<juce::DrawablePath>();
    d->setPath (curve (type, { 0, 0, 30, 16 }));
    d->setFill (juce::FillType (juce::Colours::transparentBlack));
    d->setStrokeFill (juce::FillType (colour));
    d->setStrokeType (juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    return d;
}

void draw (juce::Graphics& g, qtip::FilterType type, juce::Rectangle<float> area, juce::Colour colour, float thickness)
{
    g.setColour (colour.withAlpha (0.18f));
    g.drawHorizontalLine (juce::roundToInt (area.getCentreY()), area.getX(), area.getRight());
    g.setColour (colour);
    g.strokePath (curve (type, area), juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::PopupMenu menu (int currentType, juce::Colour colour)
{
    juce::PopupMenu m;
    for (int t = 0; t < (int) qtip::FilterType::Count; ++t)
    {
        juce::PopupMenu::Item item (qtip::filterTypeName ((qtip::FilterType) t));
        item.itemID = 1 + t;
        item.isTicked = t == currentType;
        item.image = drawable ((qtip::FilterType) t, colour);
        m.addItem (std::move (item));
    }
    return m;
}

} // namespace filtericons

// ------------------------------------------------------------------ TypeSelector
TypeSelector::TypeSelector()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Filter type");
}

void TypeSelector::attach (juce::RangedAudioParameter* p)
{
    attachment.reset();
    if (p == nullptr) return;
    attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v)
    {
        current = juce::jlimit (0, (int) qtip::FilterType::Count - 1, juce::roundToInt (v));
        repaint();
    }, nullptr);
    attachment->sendInitialUpdate();
}

void TypeSelector::detach() { attachment.reset(); }

void TypeSelector::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::well);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (isMouseOver() ? theme::border2.brighter (0.2f) : theme::border2);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);

    auto inner = getLocalBounds().reduced (8, 4);
    const auto iconArea = inner.removeFromLeft (34).toFloat().reduced (0, 3);
    filtericons::draw (g, (qtip::FilterType) current, iconArea, colour.isTransparent() ? theme::accent : colour);
    inner.removeFromLeft (8);
    g.setColour (theme::text);
    g.setFont (theme::font (13.0f, true));
    g.drawText (qtip::filterTypeName ((qtip::FilterType) current), inner.withTrimmedRight (16), juce::Justification::centredLeft);

    juce::Path arrow;
    const float ax = (float) getWidth() - 14.0f, ay = (float) getHeight() * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.0f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (theme::muted);
    g.strokePath (arrow, juce::PathStrokeType (1.5f));
}

void TypeSelector::mouseDown (const juce::MouseEvent&)
{
    juce::Component::SafePointer<TypeSelector> safe (this);
    filtericons::menu (current, colour.isTransparent() ? theme::accent : colour)
        .showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                        [safe] (int r)
                        {
                            if (safe == nullptr || r <= 0 || safe->attachment == nullptr) return;
                            safe->attachment->setValueAsCompleteGesture ((float) (r - 1));
                        });
}
