#include "PlacesBrowser.h"

#include "engine/AudioEngine.h"

namespace c2paseq
{
namespace
{
class PlaceItem final : public juce::TreeViewItem
{
public:
    PlaceItem(juce::File itemFile, bool removableRoot,
              std::function<void(const juce::File&)> remove,
              std::function<void(const juce::File&)> select,
              std::function<void()> dragStarted)
        : item(std::move(itemFile)), removable(removableRoot), onRemove(std::move(remove)),
          onSelect(std::move(select)), onDragStarted(std::move(dragStarted)) {}

    bool mightContainSubItems() override { return item.isDirectory(); }
    juce::String getUniqueName() const override { return item.getFullPathName(); }

    void paintItem(juce::Graphics& g, int width, int height) override
    {
        if (isSelected())
        {
            g.setColour(juce::Colour::fromRGB(52, 116, 139));
            g.fillRoundedRectangle(0.0f, 1.0f, static_cast<float>(width),
                                   static_cast<float>(height - 2), 3.0f);
        }
        g.setColour(item.exists() ? juce::Colour::fromRGB(218, 221, 224)
                                  : juce::Colour::fromRGB(125, 128, 132));
        g.setFont(juce::FontOptions(12.5f, removable ? juce::Font::bold : juce::Font::plain));
        g.drawFittedText(item.getFileName(), 4, 0, width - 6, height,
                         juce::Justification::centredLeft, 1);
    }

    void itemOpennessChanged(bool isNowOpen) override
    {
        if (! isNowOpen || getNumSubItems() != 0 || ! item.isDirectory())
            return;
        juce::Array<juce::File> children;
        item.findChildFiles(children, juce::File::findFilesAndDirectories, false);
        children.sort();
        for (const auto& child : children)
            if (child.isDirectory() || AudioEngine::isSupportedAudioFile(child))
                addSubItem(new PlaceItem(child, false, onRemove, onSelect, onDragStarted));
    }

    juce::var getDragSourceDescription() override
    {
        if (! AudioEngine::isSupportedAudioFile(item))
            return {};
        if (onDragStarted)
            onDragStarted();
        return juce::var("c2paseq-audio:" + item.getFullPathName());
    }

    void itemSelectionChanged(bool isNowSelected) override
    {
        if (isNowSelected && onSelect)
            onSelect(AudioEngine::isSupportedAudioFile(item) ? item : juce::File());
    }

    void itemClicked(const juce::MouseEvent& event) override
    {
        if (! removable || ! event.mods.isPopupMenu())
            return;
        juce::PopupMenu menu;
        menu.addItem(1, "Remove from Places");
        const auto target = item;
        const auto callback = onRemove;
        menu.showMenuAsync({}, [target, callback](int result)
        {
            if (result == 1 && callback)
                callback(target);
        });
    }

private:
    juce::File item;
    bool removable = false;
    std::function<void(const juce::File&)> onRemove;
    std::function<void(const juce::File&)> onSelect;
    std::function<void()> onDragStarted;
};

class RootItem final : public juce::TreeViewItem
{
public:
    bool mightContainSubItems() override { return true; }
    juce::String getUniqueName() const override { return "places-root"; }
};
}

PlacesBrowser::PlacesBrowser(PlacesStore& store) : places(store)
{
    for (auto* label : { &libraryTitle, &placesTitle })
    {
        label->setFont(juce::FontOptions(12.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, juce::Colour::fromRGB(165, 171, 178));
        addAndMakeVisible(*label);
    }
    libraryTitle.setText("LIBRARY", juce::dontSendNotification);
    placesTitle.setText("PLACES", juce::dontSendNotification);
    libraryItems.setText("Sounds\nInstruments\nAudio Effects\nPlug-ins", juce::dontSendNotification);
    libraryItems.setFont(juce::FontOptions(13.0f));
    libraryItems.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 147, 153));
    libraryItems.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(libraryItems);
    addFolder.onClick = [this] { chooseFolder(); };
    addAndMakeVisible(addFolder);
    tree.setRootItemVisible(false);
    tree.setDefaultOpenness(true);
    tree.setMultiSelectEnabled(false);
    tree.setIndentSize(14);
    tree.setColour(juce::TreeView::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(tree);
    selectedSample.setText("No sample selected", juce::dontSendNotification);
    selectedSample.setFont(juce::FontOptions(11.5f));
    selectedSample.setColour(juce::Label::textColourId, juce::Colour::fromRGB(173, 179, 184));
    selectedSample.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(selectedSample);
    previewButton.setEnabled(false);
    previewButton.setWantsKeyboardFocus(false);
    previewButton.setTooltip("Preview the selected sample without importing it");
    previewButton.onClick = [this]
    {
        if (previewPlaying && previewFile == selectedFile)
        {
            if (onPreviewStopRequested)
                onPreviewStopRequested();
            return;
        }
        if (selectedFile.existsAsFile() && onPreviewRequested)
            onPreviewRequested(selectedFile);
    };
    addAndMakeVisible(previewButton);
    rebuildTree();
}

void PlacesBrowser::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(50, 53, 57));
    g.setColour(juce::Colour::fromRGB(75, 79, 84));
    g.drawVerticalLine(getWidth() - 1, 0.0f, static_cast<float>(getHeight()));
}

void PlacesBrowser::resized()
{
    auto area = getLocalBounds().reduced(10, 8);
    libraryTitle.setBounds(area.removeFromTop(24));
    libraryItems.setBounds(area.removeFromTop(104));
    area.removeFromTop(8);
    placesTitle.setBounds(area.removeFromTop(24));
    addFolder.setBounds(area.removeFromTop(28));
    area.removeFromTop(4);
    auto previewArea = area.removeFromBottom(58);
    tree.setBounds(area);
    previewArea.removeFromTop(4);
    selectedSample.setBounds(previewArea.removeFromTop(22));
    previewButton.setBounds(previewArea.removeFromTop(28));
}

void PlacesBrowser::setPreviewState(const juce::File& activeFile, bool playing)
{
    previewFile = activeFile;
    previewPlaying = playing && activeFile.existsAsFile();
    previewButton.setButtonText(previewPlaying && previewFile == selectedFile
                                    ? "Stop" : "Preview");
    previewButton.setEnabled(selectedFile.existsAsFile());
    if (selectedFile.existsAsFile())
        selectedSample.setText((previewPlaying && previewFile == selectedFile
                                    ? "Playing: " : "Selected: ")
                                   + selectedFile.getFileName(),
                               juce::dontSendNotification);
    else
        selectedSample.setText("No sample selected", juce::dontSendNotification);
}

void PlacesBrowser::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser>("Add Sample Folder",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory));
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectDirectories,
        [safe = juce::Component::SafePointer<PlacesBrowser>(this)](const juce::FileChooser& c)
        {
            if (safe == nullptr || c.getResult() == juce::File())
                return;
            const auto result = safe->places.addFolder(c.getResult());
            if (safe->onStatus)
                safe->onStatus(result.wasOk() ? "Added " + c.getResult().getFileName()
                                              : result.getErrorMessage());
            safe->rebuildTree();
            safe->chooser.reset();
        });
}

void PlacesBrowser::rebuildTree()
{
    tree.setRootItem(nullptr);
    rootItem = std::make_unique<RootItem>();
    for (const auto& folder : places.folders())
        rootItem->addSubItem(new PlaceItem(folder, true, [this](const juce::File& target)
        {
            const auto result = places.removeFolder(target);
            if (onStatus)
                onStatus(result.wasOk() ? "Removed " + target.getFileName()
                                        : result.getErrorMessage());
            rebuildTree();
        }, [this](const juce::File& selection)
        {
            selectedFile = selection;
            setPreviewState(previewFile, previewPlaying);
        }, [this]
        {
            if (onAudioDragStarted)
                onAudioDragStarted();
        }));
    tree.setRootItem(rootItem.get());
}
}
