#pragma once

#include <juce_core/juce_core.h>

namespace c2paseq
{
class PlacesStore
{
public:
    PlacesStore();
    explicit PlacesStore(juce::File storageFile);

    [[nodiscard]] const juce::Array<juce::File>& folders() const noexcept;
    [[nodiscard]] juce::Result addFolder(const juce::File& folder);
    [[nodiscard]] juce::Result removeFolder(const juce::File& folder);

private:
    void load();
    [[nodiscard]] juce::Result save() const;

    juce::File file;
    juce::Array<juce::File> roots;
};
}
