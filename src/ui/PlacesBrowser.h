#pragma once

#include "PlacesStore.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace c2paseq
{
class PlacesBrowser final : public juce::Component
{
public:
    explicit PlacesBrowser(PlacesStore& store);

    void paint(juce::Graphics&) override;
    void resized() override;
    void setPreviewState(const juce::File&, bool playing);
    std::function<void(const juce::String&)> onStatus;
    std::function<void(const juce::File&)> onPreviewRequested;
    std::function<void()> onPreviewStopRequested;
    std::function<void()> onAudioDragStarted;

private:
    void chooseFolder();
    void rebuildTree();

    PlacesStore& places;
    juce::Label libraryTitle;
    juce::Label placesTitle;
    juce::Label libraryItems;
    juce::TextButton addFolder { "+ Add Folder..." };
    juce::TreeView tree;
    juce::Label selectedSample;
    juce::TextButton previewButton { "Preview" };
    std::unique_ptr<juce::TreeViewItem> rootItem;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File selectedFile;
    juce::File previewFile;
    bool previewPlaying = false;
};
}
