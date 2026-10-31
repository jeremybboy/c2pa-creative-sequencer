#pragma once
#include "provenance/CredentialViewModel.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace c2paseq
{
class CredentialsPanel final : public juce::Component
{
public:
    CredentialsPanel();
    ~CredentialsPanel() override;
    void setClip(CredentialViewModel view, const juce::String& trackName);
    void setEmptySelection(bool multiple);
    void setLastExport(std::optional<CredentialViewModel> view);
    void showLastExport();
    void paint(juce::Graphics&) override;
    void resized() override;
    std::function<void()> onClose;
private:
    void updateContent();
    void showDetails(const juce::String& text);
    std::optional<CredentialViewModel> clip, lastExport;
    juce::String track, emptyMessage = "Select one audio clip to inspect its Content Credentials.";
    bool exportSelected = false, detailsSelected = false;
    juce::TextButton clipTab { "Clip" }, exportTab { "Last Export" }, detailsTab { "Details" };
    juce::TextButton fullDetails { "View Full Details" }, close { "Close" };
    std::array<juce::TextButton, 3> checkDetails;
    juce::TextEditor details;
    juce::TreeView tree;
    std::unique_ptr<juce::TreeViewItem> root;
    juce::Rectangle<int> summaryBounds, checksBounds, historyBounds;
};
}
