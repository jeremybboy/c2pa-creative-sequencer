#include "IconButton.h"

namespace c2paseq
{
IconButton::IconButton(juce::String name, Icon initialIcon)
    : Button(std::move(name)), icon(initialIcon)
{
}

void IconButton::setIcon(Icon newIcon)
{
    if (icon == newIcon)
        return;
    icon = newIcon;
    repaint();
}

void IconButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    auto background = findColour(juce::TextButton::buttonColourId);
    getLookAndFeel().drawButtonBackground(g, *this, background, highlighted, down);
    auto colour = findColour(getToggleState() ? juce::TextButton::textColourOnId
                                               : juce::TextButton::textColourOffId);
    if (! isEnabled())
        colour = colour.withMultipliedAlpha(0.42f);
    drawIcon(g, getLocalBounds().toFloat().reduced(7.0f), icon, colour);
}

void IconButton::drawIcon(juce::Graphics& g, juce::Rectangle<float> area,
                          Icon value, juce::Colour colour)
{
    g.setColour(colour);
    const auto stroke = juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded);
    juce::Path path;
    const auto cx = area.getCentreX();
    const auto cy = area.getCentreY();

    switch (value)
    {
        case Icon::play:
            path.addTriangle(area.getX() + area.getWidth() * 0.26f, area.getY() + 1.0f,
                             area.getRight() - 1.0f, cy,
                             area.getX() + area.getWidth() * 0.26f, area.getBottom() - 1.0f);
            g.fillPath(path);
            break;
        case Icon::pause:
            g.fillRoundedRectangle(area.getX() + area.getWidth() * 0.22f, area.getY() + 1.0f,
                                   area.getWidth() * 0.2f, area.getHeight() - 2.0f, 1.0f);
            g.fillRoundedRectangle(area.getX() + area.getWidth() * 0.58f, area.getY() + 1.0f,
                                   area.getWidth() * 0.2f, area.getHeight() - 2.0f, 1.0f);
            break;
        case Icon::stop:
            g.fillRoundedRectangle(area.reduced(2.0f), 2.0f);
            break;
        case Icon::record:
            g.setColour(juce::Colour::fromRGB(229, 79, 88).withMultipliedAlpha(colour.getFloatAlpha()));
            g.fillEllipse(area.reduced(2.0f));
            break;
        case Icon::loop:
            path.startNewSubPath(area.getX() + 2.0f, cy - 3.0f);
            path.lineTo(area.getRight() - 4.0f, cy - 3.0f);
            path.lineTo(area.getRight() - 7.0f, cy - 6.0f);
            path.startNewSubPath(area.getRight() - 2.0f, cy + 3.0f);
            path.lineTo(area.getX() + 4.0f, cy + 3.0f);
            path.lineTo(area.getX() + 7.0f, cy + 6.0f);
            g.strokePath(path, stroke);
            break;
        case Icon::undo:
        case Icon::redo:
        {
            const auto direction = value == Icon::undo ? -1.0f : 1.0f;
            path.startNewSubPath(cx - direction * 5.0f, area.getY() + 3.0f);
            path.cubicTo(cx + direction * 5.0f, area.getY() + 2.0f,
                         cx + direction * 7.0f, area.getBottom() - 2.0f,
                         cx + direction * 1.0f, area.getBottom() - 2.0f);
            g.strokePath(path, stroke);
            juce::Path arrow;
            arrow.addTriangle(cx - direction * 7.0f, area.getY() + 3.0f,
                              cx - direction * 2.0f, area.getY(),
                              cx - direction * 2.0f, area.getY() + 7.0f);
            g.fillPath(arrow);
            break;
        }
        case Icon::zoomIn:
        case Icon::zoomOut:
            g.drawEllipse(area.getX() + 1.0f, area.getY() + 1.0f,
                          area.getWidth() * 0.64f, area.getHeight() * 0.64f, 1.8f);
            g.drawLine(cx + 1.0f, cy + 1.0f, area.getRight() - 1.0f,
                       area.getBottom() - 1.0f, 1.8f);
            g.drawLine(area.getX() + 4.0f, area.getY() + area.getHeight() * 0.32f,
                       area.getX() + area.getWidth() * 0.55f,
                       area.getY() + area.getHeight() * 0.32f, 1.5f);
            if (value == Icon::zoomIn)
                g.drawLine(area.getX() + area.getWidth() * 0.32f, area.getY() + 4.0f,
                           area.getX() + area.getWidth() * 0.32f,
                           area.getY() + area.getHeight() * 0.55f, 1.5f);
            break;
        case Icon::audio:
            path.startNewSubPath(area.getX() + 1.0f, cy - 3.0f);
            path.lineTo(area.getX() + 5.0f, cy - 3.0f);
            path.lineTo(cx, area.getY() + 1.0f);
            path.lineTo(cx, area.getBottom() - 1.0f);
            path.lineTo(area.getX() + 5.0f, cy + 3.0f);
            path.lineTo(area.getX() + 1.0f, cy + 3.0f);
            path.closeSubPath();
            g.fillPath(path);
            path.clear();
            path.addArc(cx - 1.0f, area.getY() + 3.0f, area.getWidth() * 0.62f,
                        area.getHeight() - 6.0f, -0.8f, 0.8f, true);
            g.strokePath(path, juce::PathStrokeType(1.5f));
            break;
        case Icon::keyboard:
        {
            const auto keys = area.reduced(1.0f);
            g.drawRoundedRectangle(keys, 1.5f, 1.5f);
            const auto whiteWidth = keys.getWidth() / 7.0f;
            for (int index = 1; index < 7; ++index)
                g.drawVerticalLine(static_cast<int>(keys.getX() + whiteWidth * index),
                                   keys.getY(), keys.getBottom());
            for (const auto index : { 1, 2, 4, 5, 6 })
                g.fillRect(keys.getX() + whiteWidth * index - whiteWidth * 0.24f,
                           keys.getY(), whiteWidth * 0.48f, keys.getHeight() * 0.58f);
            break;
        }
        case Icon::close:
            g.drawLine(area.getX() + 2.0f, area.getY() + 2.0f,
                       area.getRight() - 2.0f, area.getBottom() - 2.0f, 1.8f);
            g.drawLine(area.getRight() - 2.0f, area.getY() + 2.0f,
                       area.getX() + 2.0f, area.getBottom() - 2.0f, 1.8f);
            break;
    }
}
}
