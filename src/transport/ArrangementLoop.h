#pragma once

#include "project/Project.h"

namespace c2paseq
{
struct ArrangementLoopRange
{
    double startSeconds = 0.0;
    double endSeconds = 0.0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return startSeconds >= 0.0 && endSeconds > startSeconds + 0.001;
    }
};

// A selected clip defines the range; without a selection, use the arrangement.
[[nodiscard]] ArrangementLoopRange resolveArrangementLoopRange(
    const Project&, const juce::String& selectedClipId);
}
