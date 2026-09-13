#pragma once

#include <juce_core/juce_core.h>

namespace c2paseq
{
class ProjectPaths final
{
public:
    explicit ProjectPaths(juce::File projectDirectory);

    [[nodiscard]] const juce::File& root() const noexcept;
    [[nodiscard]] juce::File projectJson() const;
    [[nodiscard]] juce::File arrangementEdit() const;
    [[nodiscard]] juce::File provenanceJson() const;
    [[nodiscard]] juce::File mediaDirectory() const;
    [[nodiscard]] juce::Result createDirectories() const;
    [[nodiscard]] static bool hasProjectExtension(const juce::File& file);

private:
    juce::File projectRoot;
};
}
