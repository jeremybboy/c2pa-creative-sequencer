#pragma once

#include "recording/AudioInputChoice.h"

#include <algorithm>
#include <vector>

namespace c2paseq
{
struct InputDeviceGroup
{
    juce::String deviceName;
    std::vector<std::size_t> inputIndices;
    bool advanced() const noexcept { return inputIndices.size() > 8; }
};

inline std::vector<InputDeviceGroup> groupRecordingInputs(
    const std::vector<AudioInputChoice>& inputs)
{
    std::vector<InputDeviceGroup> groups;
    for (std::size_t index = 0; index < inputs.size(); ++index)
    {
        const auto found = std::find_if(groups.begin(), groups.end(), [&](const auto& group)
        { return group.deviceName == inputs[index].deviceName; });
        if (found == groups.end()) groups.push_back({ inputs[index].deviceName, { index } });
        else found->inputIndices.push_back(index);
    }
    // Channel count is a presentation heuristic, not a hardware classification.
    // No names are blacklisted and no detected input is discarded.
    std::stable_sort(groups.begin(), groups.end(), [](const auto& left, const auto& right)
    { return left.inputIndices.size() < right.inputIndices.size(); });
    return groups;
}

inline bool sameRecordingInput(const AudioInputChoice& left, const AudioInputChoice& right)
{
    return left.deviceName == right.deviceName && left.channelIndex == right.channelIndex;
}
}
