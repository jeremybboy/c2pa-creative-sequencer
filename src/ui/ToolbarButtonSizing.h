#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace c2paseq
{
inline int toolbarButtonSlotWidth(juce::TextButton& button, int slotHeight,
                                 const juce::String& alternateLabel = {})
{
    const auto font = button.getLookAndFeel().getTextButtonFont(button, slotHeight - 2);
    const auto width = std::max(juce::GlyphArrangement::getStringWidthInt(font, button.getButtonText()),
        juce::GlyphArrangement::getStringWidthInt(font, alternateLabel));
    // Two pixels between slots, twelve pixels of paint padding, two spare pixels.
    return std::max(40, width + 16);
}
}
