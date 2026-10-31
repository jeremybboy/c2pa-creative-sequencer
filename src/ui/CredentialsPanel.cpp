#include "CredentialsPanel.h"

namespace c2paseq
{
namespace
{
juce::Colour checkColour(CredentialCheckState state)
{
    switch (state)
    {
        case CredentialCheckState::pass: return juce::Colour(0xff72d89c);
        case CredentialCheckState::fail: return juce::Colour(0xfff28a86);
        case CredentialCheckState::warning: return juce::Colour(0xffe9bf6a);
        default: return juce::Colour(0xffb0bdc8);
    }
}
juce::String checkName(CredentialCheckState state)
{
    switch (state)
    {
        case CredentialCheckState::pass: return "PASS";
        case CredentialCheckState::fail: return "FAIL";
        case CredentialCheckState::warning: return "WARNING";
        case CredentialCheckState::notApplicable: return "N/A";
        default: return "UNKNOWN";
    }
}
class HistoryItem final : public juce::TreeViewItem
{
public:
    HistoryItem(CredentialHistoryNode value, std::function<void(const juce::String&)> callback)
        : node(std::move(value)), selected(std::move(callback))
    {
        for (auto& child : node.children) addSubItem(new HistoryItem(child, selected));
    }
    bool mightContainSubItems() override { return getNumSubItems() > 0; }
    int getItemHeight() const override { return 46; }
    void paintItem(juce::Graphics& g, int width, int height) override
    {
        const juce::Rectangle<int> bounds(0, 2, std::max(0, width - 4), height - 4);
        g.setColour(juce::Colour(0xff29333c)); g.fillRoundedRectangle(bounds.toFloat(), 4);
        g.setColour(juce::Colour(0xff71818e)); g.drawRoundedRectangle(bounds.toFloat(), 4, 1);
        g.setColour(juce::Colour(0xffedf1f3)); g.setFont(juce::FontOptions(12, juce::Font::bold));
        g.drawText(node.title, bounds.reduced(8, 2).removeFromTop(17), juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(11)); g.setColour(juce::Colour(0xffbfccd5));
        g.drawText(node.subtitle, bounds.reduced(8, 2).withTrimmedTop(18), juce::Justification::centredLeft);
    }
    juce::String getTooltip() override { return node.title + "\n" + node.subtitle + "\nClick for details; expand for available history."; }
    void itemClicked(const juce::MouseEvent&) override
    { if (selected) selected(node.title + "\n" + node.subtitle + "\n\n" + node.details); }
private:
    CredentialHistoryNode node;
    std::function<void(const juce::String&)> selected;
};
}
CredentialsPanel::CredentialsPanel()
{
    for (auto* button : { &clipTab, &exportTab, &detailsTab, &fullDetails, &close, &expand }) addAndMakeVisible(button);
    addChildComponent(graph);
    expand.onClick = [this] { setExpanded(! expanded); };
    for (auto& button : checkDetails)
    { button.setButtonText("Details"); addAndMakeVisible(button); button.setWantsKeyboardFocus(false); }
    clipTab.onClick = [this] { exportSelected = false; detailsSelected = false; updateContent(); };
    exportTab.onClick = [this] { exportSelected = true; detailsSelected = false; updateContent(); };
    detailsTab.onClick = [this]
    {
        const auto& current = exportSelected ? lastExport : clip;
        showDetails(current ? current->details : emptyMessage);
    };
    fullDetails.onClick = detailsTab.onClick;
    for (size_t index = 0; index < checkDetails.size(); ++index)
        checkDetails[index].onClick = [this, index]
        {
            const auto& current = exportSelected ? lastExport : clip;
            if (! current) return;
            const auto& check = current->checks[index];
            showDetails(check.title + "\n" + check.summary + "\n\n"
                + (check.evidence.isEmpty() ? "No separate SDK evidence was reported." : check.evidence)
                + "\n" + current->policy);
        };
    close.onClick = [this] { if (onClose) onClose(); };
    details.setMultiLine(true); details.setReadOnly(true); details.setScrollbarsShown(true);
    details.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1c242b));
    details.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe4eaf0));
    addChildComponent(details);
    tree.setColour(juce::TreeView::backgroundColourId, juce::Colour(0xff1b2228));
    tree.setColour(juce::TreeView::linesColourId, juce::Colour(0xff8295a3));
    tree.setRootItemVisible(true); tree.setDefaultOpenness(false); addAndMakeVisible(tree);
    updateContent();
}
CredentialsPanel::~CredentialsPanel() { tree.setRootItem(nullptr); }
void CredentialsPanel::setClip(CredentialViewModel view, const juce::String& trackName)
{ clip = std::move(view); track = trackName; if (! exportSelected) updateContent(); }
void CredentialsPanel::setEmptySelection(bool multiple)
{
    clip.reset(); emptyMessage = multiple ? "Multiple clips selected - select one asset to inspect."
        : "Select one audio clip to inspect its Content Credentials.";
    if (! exportSelected) updateContent();
}
void CredentialsPanel::setLastExport(std::optional<CredentialViewModel> view)
{ lastExport = std::move(view); if (exportSelected) updateContent(); }
void CredentialsPanel::showLastExport() { exportSelected = true; detailsSelected = false; updateContent(); }
void CredentialsPanel::setExpanded(bool enabled)
{
    expanded = enabled; detailsSelected = false;
    expand.setButtonText(expanded ? "Back to Arrangement" : "Expand Provenance");
    updateContent();
    if (onExpansionChanged) onExpansionChanged();
}
void CredentialsPanel::showDetails(const juce::String& text)
{ detailsSelected = true; details.setText(text, false); resized(); repaint(); }
void CredentialsPanel::updateContent()
{
    tree.setRootItem(nullptr); root.reset();
    const auto& current = exportSelected ? lastExport : clip;
    if (current)
    {
        graph.setHistory(current->history);
        root = std::make_unique<HistoryItem>(current->history,
            [this](const auto& text) { showDetails(text); });
        tree.setRootItem(root.get()); root->setOpen(true);
    }
    if (detailsSelected) details.setText(current ? current->details : emptyMessage, false);
    resized(); repaint();
}
void CredentialsPanel::resized()
{
    auto area = getLocalBounds().reduced(12, 8);
    auto top = area.removeFromTop(28); top.removeFromLeft(180);
    clipTab.setBounds(top.removeFromLeft(62).reduced(2));
    exportTab.setBounds(top.removeFromLeft(102).reduced(2));
    detailsTab.setBounds(top.removeFromLeft(74).reduced(2));
    expand.setBounds(top.removeFromLeft(166).reduced(2));
    close.setBounds(top.removeFromRight(64).reduced(2));
    area.removeFromTop(10);
    details.setBounds(area); details.setVisible(detailsSelected);
    const auto& current = exportSelected ? lastExport : clip;
    expand.setEnabled(current.has_value() || expanded);
    graph.setBounds(area); graph.setVisible(expanded && ! detailsSelected && current.has_value());
    summaryBounds = area.removeFromLeft(juce::jlimit(220, 300, getWidth() / 4));
    area.removeFromLeft(12);
    checksBounds = area.removeFromLeft(juce::jlimit(300, 360, getWidth() / 3));
    area.removeFromLeft(12); historyBounds = area;
    tree.setBounds(historyBounds.withTrimmedTop(24));
    tree.setVisible(! detailsSelected && ! expanded);
    fullDetails.setBounds(summaryBounds.withTrimmedTop(summaryBounds.getHeight() - 30).withWidth(164));
    fullDetails.setVisible(current.has_value() && ! detailsSelected && ! expanded);
    for (int i = 0; i < 3; ++i)
    {
        auto bounds = checksBounds.withTrimmedTop(24 + i * 50).withHeight(46);
        checkDetails[static_cast<size_t>(i)].setBounds(bounds.removeFromRight(62).reduced(2, 8));
        checkDetails[static_cast<size_t>(i)].setVisible(current.has_value() && ! detailsSelected && ! expanded);
    }
    clipTab.setToggleState(! exportSelected && ! detailsSelected, juce::dontSendNotification);
    exportTab.setToggleState(exportSelected && ! detailsSelected, juce::dontSendNotification);
    detailsTab.setToggleState(detailsSelected, juce::dontSendNotification);
}
void CredentialsPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a2026));
    g.setColour(juce::Colour(0xff73838d)); g.drawHorizontalLine(0, 0, static_cast<float>(getWidth()));
    g.setFont(juce::FontOptions(15, juce::Font::bold)); g.setColour(juce::Colours::white);
    g.drawText(expanded ? "Provenance Explorer" : "Content Credentials", 14, 8, 180, 28, juce::Justification::centredLeft);
    if (detailsSelected) return;
    const auto& current = exportSelected ? lastExport : clip;
    if (! current)
    {
        g.setFont(juce::FontOptions(14));
        g.drawText(exportSelected ? "No completed signed export in this session." : emptyMessage,
                   getLocalBounds().reduced(20).withTrimmedTop(40), juce::Justification::centred);
        return;
    }
    if (expanded) return;
    const auto& view = *current;
    auto summary = summaryBounds;
    const auto icon = summary.removeFromTop(22).removeFromLeft(18).toFloat().reduced(2);
    g.setColour(juce::Colour(0xff8cb6ca)); g.drawRect(icon, 1.0f);
    for (int line = 0; line < 3; ++line)
        g.drawHorizontalLine(juce::roundToInt(icon.getY() + 5 + line * 4), icon.getX() + 3, icon.getRight() - 3);
    g.setFont(juce::FontOptions(14, juce::Font::bold));
    g.drawText(view.title, summary.removeFromTop(24), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(12)); g.setColour(juce::Colour(0xffbfccd5));
    g.drawText(exportSelected ? "Completed signed export (session snapshot)" : "Track: " + track,
               summary.removeFromTop(22), juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffd3e0e8)); g.setFont(juce::FontOptions(13, juce::Font::bold));
    g.drawText(view.badge, summary.removeFromTop(24), juce::Justification::centredLeft);
    summary.removeFromTop(6); summary.removeFromBottom(34);
    g.setFont(juce::FontOptions(12));
    g.drawFittedText(view.description, summary, juce::Justification::topLeft, 5, 1.0f);
    g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(12, juce::Font::bold));
    g.drawText("Validation", checksBounds.withHeight(20), juce::Justification::centredLeft);
    g.drawText("How this audio was made", historyBounds.withHeight(20), juce::Justification::centredLeft);
    for (int i = 0; i < 3; ++i)
    {
        const auto& check = view.checks[static_cast<size_t>(i)];
        auto card = checksBounds.withTrimmedTop(24 + i * 50).withHeight(46);
        g.setColour(juce::Colour(0xff283038)); g.fillRoundedRectangle(card.toFloat(), 4);
        card.removeFromRight(66); card.reduce(8, 3);
        g.setColour(checkColour(check.state)); g.setFont(juce::FontOptions(12, juce::Font::bold));
        g.drawText(check.title + " - " + checkName(check.state), card.removeFromTop(18), juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(11));
        g.drawText(check.summary, card, juce::Justification::centredLeft);
    }
    auto policy = checksBounds.withTrimmedTop(178);
    g.setColour(juce::Colour(0xffbdc7cf)); g.setFont(juce::FontOptions(11));
    g.drawFittedText(view.policy, policy, juce::Justification::topLeft, 3, 1.0f);
}
}
