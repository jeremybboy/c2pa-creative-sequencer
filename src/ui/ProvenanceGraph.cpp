#include "ProvenanceGraph.h"
#include <cmath>

namespace c2paseq
{
namespace
{
constexpr float cardWidth = 360, cardHeight = 240, siblingGap = 48, levelPitch = 330;
juce::String sourceLabel(const juce::String& source)
{
    const auto term = source.fromLastOccurrenceOf("/", false, false);
    if (source != "http://cv.iptc.org/newscodes/digitalsourcetype/" + term
        && source != "https://cv.iptc.org/newscodes/digitalsourcetype/" + term)
        return "Declared source: " + source;
    if (term == "digitalCapture") return "Contains Captured Media";
    if (term == "digitalCreation") return "Contains Non-GenAI digital creation";
    if (term == "trainedAlgorithmicMedia") return "Contains GenAI Media";
    if (term == "compositeWithTrainedAlgorithmicMedia") return "Contains GenAI edits";
    // Translate explicit IPTC declarations only; never infer from filenames or audio.
    return "Declared source: " + term;
}
ProvenanceGraphNode convert(const CredentialHistoryNode& value, const juce::String& id)
{
    ProvenanceGraphNode node;
    node.id = id; node.title = value.title; node.generator = value.generator;
    node.relationship = value.relationship; node.technical = value.details;
    node.hasManifest = value.embeddedManifest;
    node.hasManifestReference = value.referencedManifest.isNotEmpty();
    node.instrument = value.instrument;
    node.credentialsAbsent = value.credentialsAbsent;
    node.sourceUnavailable = value.kind == CredentialNodeKind::ingredient && ! value.sourceAvailable;
    node.availability = value.subtitle;
    for (const auto& source : value.declaredSources) node.sources.addIfNotAlreadyThere(sourceLabel(source));
    int index = 0;
    for (const auto& child : value.children)
    {
        if (child.kind == CredentialNodeKind::action) node.actions.add(child.title);
        else if (value.kind == CredentialNodeKind::ingredient && child.kind == CredentialNodeKind::asset)
        {
            // A linked manifest describes this ingredient, not a second imaginary audio asset.
            auto embedded = convert(child, id + "/manifest");
            node.generator = embedded.generator; node.hasManifest = embedded.hasManifest;
            node.instrument = embedded.instrument;
            node.sources.addArray(embedded.sources); node.sources.removeDuplicates(false);
            node.actions.addArray(embedded.actions);
            node.children = std::move(embedded.children);
            node.technical += "\n\nEmbedded history:\n" + embedded.technical;
        }
        else node.children.push_back(convert(child, id + "/" + juce::String(index++)));
    }
    node.details = node.title;
    if (node.relationship.isNotEmpty()) node.details += "\nRelationship: " + node.relationship;
    if (node.generator.isNotEmpty()) node.details += "\nClaim generator: " + node.generator;
    if (node.availability != "Claim generator: " + node.generator) node.details += "\n\n" + node.availability;
    if (node.instrument.isNotEmpty()) node.details += "\nInstrument: " + node.instrument;
    if (! node.sources.isEmpty()) node.details += "\n\n" + node.sources.joinIntoString("\n");
    if (! node.actions.isEmpty()) node.details += "\n\nDeclared actions / tools:\n" + node.actions.joinIntoString("\n");
    node.details += "\n\nSource labels describe manifest declarations, not audio detection."
        "\nEmbedded history is not independent validation of the ingredient audio.";
    return node;
}
float span(const ProvenanceGraphNode& node, const std::set<juce::String>& collapsed)
{
    if (node.children.empty() || collapsed.contains(node.id)) return cardWidth;
    float width = -siblingGap;
    for (const auto& child : node.children) width += span(child, collapsed) + siblingGap;
    return std::max(cardWidth, width);
}
void place(const ProvenanceGraphNode& node, const std::set<juce::String>& collapsed,
           float x, float y, int parent, std::vector<ProvenanceGraphPlacement>& out)
{
    const auto width = span(node, collapsed);
    const auto index = static_cast<int>(out.size());
    out.push_back({ &node, { x + (width - cardWidth) * 0.5f, y, cardWidth, cardHeight }, parent });
    if (collapsed.contains(node.id)) return;
    for (const auto& child : node.children)
    { place(child, collapsed, x, y + levelPitch, index, out); x += span(child, collapsed) + siblingGap; }
}
}
ProvenanceGraphNode makeProvenanceGraph(const CredentialHistoryNode& root) { return convert(root, "root"); }
std::vector<ProvenanceGraphPlacement> layoutProvenanceGraph(
    const ProvenanceGraphNode& root, const std::set<juce::String>& collapsed)
{
    std::vector<ProvenanceGraphPlacement> result; place(root, collapsed, 0, 0, -1, result); return result;
}
void ProvenanceGraphCamera::zoomAt(float factor, juce::Point<float> anchor)
{
    if (! std::isfinite(factor) || factor <= 0) return;
    const auto fixed = toWorld(anchor);
    zoom = juce::jlimit(0.08f, 4.0f, zoom * factor);
    translation = anchor - fixed * zoom;
}
void ProvenanceGraphCamera::fit(juce::Rectangle<float> content, juce::Rectangle<float> viewport)
{
    if (content.isEmpty() || viewport.isEmpty()) return;
    zoom = juce::jlimit(0.08f, 1.25f,
        std::min(viewport.getWidth() / content.getWidth(), viewport.getHeight() / content.getHeight()));
    translation = viewport.getCentre() - content.getCentre() * zoom;
}

class ProvenanceGraphView::Canvas final : public juce::Component
{
public:
    explicit Canvas(ProvenanceGraphView& view) : owner(view)
    { setMouseCursor(juce::MouseCursor::DraggingHandCursor); setWantsKeyboardFocus(true); }
    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(0xff0b0b0c));
        juce::Graphics::ScopedSaveState saved(graphics);
        graphics.addTransform(juce::AffineTransform::scale(owner.camera.zoom)
            .translated(owner.camera.translation.x, owner.camera.translation.y));
        for (const auto& placed : owner.nodes)
        {
            if (placed.parent < 0) continue;
            const auto& parent = owner.nodes[static_cast<size_t>(placed.parent)];
            const juce::Point<float> start(parent.bounds.getCentreX(), parent.bounds.getBottom());
            const juce::Point<float> end(placed.bounds.getCentreX(), placed.bounds.getY());
            juce::Path link; link.startNewSubPath(start);
            link.cubicTo(start.x, start.y + 40, end.x, end.y - 40, end.x, end.y);
            graphics.setColour(juce::Colour(0xff8398a6));
            if (placed.node->sourceUnavailable)
            {
                juce::Path dashed; const float pattern[] { 6, 5 };
                juce::PathStrokeType(1.5f).createDashedStroke(dashed, link, pattern, 2);
                graphics.fillPath(dashed);
            }
            else graphics.strokePath(link, juce::PathStrokeType(1.5f));
            graphics.setFont(juce::FontOptions(14));
            graphics.drawText(placed.node->relationship == "componentOf" ? "Component of" : placed.node->relationship,
                              juce::Rectangle<float>(end.x - 150, end.y - 26, 300, 20),
                              juce::Justification::centred);
        }
        for (const auto& placed : owner.nodes)
        {
            const auto& node = *placed.node;
            auto bounds = placed.bounds;
            graphics.setColour(juce::Colour(0xff26282b)); graphics.fillRoundedRectangle(bounds, 9);
            graphics.setColour(node.id == owner.selectedId ? juce::Colour(0xffa9d0ff)
                : node.hasManifest ? juce::Colour(0xff438aff) : juce::Colour(0xff858a92));
            graphics.drawRoundedRectangle(bounds, 9, node.id == owner.selectedId ? 3.0f : 1.8f);
            auto text = bounds.reduced(18, 14);
            auto heading = text.removeFromTop(30);
            // A schematic audio symbol, not a fabricated waveform or a playback control.
            auto audio = heading.removeFromLeft(34).reduced(3, 2);
            graphics.setColour(juce::Colour(0xff13171c)); graphics.fillRoundedRectangle(audio, 4);
            graphics.setColour(juce::Colour(0xff9bc6ff));
            for (int bar = 0; bar < 5; ++bar)
            {
                const float height = 5.0f + static_cast<float>(2 - std::abs(bar - 2)) * 5.0f;
                const auto x = audio.getX() + 5 + static_cast<float>(bar) * 4.0f;
                graphics.drawLine(x, audio.getCentreY() - height * 0.5f, x, audio.getCentreY() + height * 0.5f, 2);
            }
            heading.removeFromLeft(8);
            graphics.setFont(juce::FontOptions(13, juce::Font::bold));
            graphics.drawText(placed.parent < 0 ? "SELECTED AUDIO" : node.relationship.isNotEmpty()
                ? "AUDIO INGREDIENT" : "EMBEDDED HISTORY", heading, juce::Justification::centredLeft);
            graphics.setColour(juce::Colours::white); graphics.setFont(juce::FontOptions(20, juce::Font::bold));
            graphics.drawFittedText(node.title, text.removeFromTop(52).toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
            graphics.setFont(juce::FontOptions(16)); graphics.setColour(juce::Colour(0xffe1e3e7));
            graphics.drawText(node.generator.isNotEmpty() ? node.generator
                : node.hasManifest ? "Claim generator not reported" : node.hasManifestReference
                    ? "Referenced manifest is not embedded" : node.credentialsAbsent
                        ? "No Content Credentials" : "Manifest / history unavailable",
                text.removeFromTop(26), juce::Justification::centredLeft);
            auto pill = text.removeFromTop(30).reduced(0, 2);
            graphics.setColour(juce::Colour(0xff111e32)); graphics.fillRoundedRectangle(pill, 9);
            graphics.setColour(juce::Colour(0xffa3caff)); graphics.setFont(juce::FontOptions(15, juce::Font::bold));
            graphics.drawText(node.sources.isEmpty() ? "Source type not declared" : node.sources[0],
                              pill.reduced(8, 0), juce::Justification::centredLeft);
            if (node.instrument.isNotEmpty())
            {
                graphics.setFont(juce::FontOptions(16));
                graphics.drawText("Instrument: " + node.instrument, text.removeFromTop(24), juce::Justification::centredLeft);
            }
            graphics.setColour(juce::Colour(0xffc1c5cd)); graphics.setFont(juce::FontOptions(13));
            graphics.drawText(node.sourceUnavailable ? node.credentialsAbsent
                ? "No CC - source audio unavailable" : "Source unavailable - history only"
                : node.hasManifest ? "Embedded credentials - see validation separately"
                    : node.hasManifestReference ? "Credentials referenced - not embedded"
                        : node.credentialsAbsent ? "No Content Credentials" : "Credentials unknown / not evaluated",
                text.removeFromTop(22), juce::Justification::centredLeft);
            if (! node.children.empty())
            {
                graphics.setColour(juce::Colour(0xffa3caff));
                graphics.drawText(owner.collapsed.contains(node.id) ? "+ Expand earlier contributions"
                    : "- Collapse contributions", text, juce::Justification::centredLeft);
            }
        }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        dragStart = owner.camera.translation; grabKeyboardFocus();
        const auto world = owner.camera.toWorld(e.position);
        for (const auto& node : owner.nodes)
            if (node.bounds.contains(world))
            {
                owner.selectNode(node.node->id);
                if (! node.node->children.empty() && world.y > node.bounds.getBottom() - 34)
                    owner.toggleBranch(node.node->id);
                return;
            }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    { owner.camera.translation = dragStart + e.getOffsetFromDragStart().toFloat(); repaint(); }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        const auto world = owner.camera.toWorld(e.position);
        for (const auto& node : owner.nodes)
            if (node.bounds.contains(world)) { owner.toggleBranch(node.node->id); return; }
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (e.mods.isCommandDown() || e.mods.isCtrlDown()) owner.zoomAt(std::exp(wheel.deltaY * 1.5f), e.position);
        else { owner.camera.translation += juce::Point<float>(wheel.deltaX * 260, wheel.deltaY * 260); repaint(); }
    }
    void mouseMagnify(const juce::MouseEvent& e, float scale) override { owner.zoomAt(scale, e.position); }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key.getTextCharacter() == '+' || key.getTextCharacter() == '=') { owner.zoomAt(1.2f, getLocalBounds().getCentre().toFloat()); return true; }
        if (key.getTextCharacter() == '-') { owner.zoomAt(1 / 1.2f, getLocalBounds().getCentre().toFloat()); return true; }
        if (key.getTextCharacter() == '0' || key.getTextCharacter() == 'f') { owner.fit(); return true; }
        return false;
    }
private:
    ProvenanceGraphView& owner;
    juce::Point<float> dragStart;
};
ProvenanceGraphView::ProvenanceGraphView() : canvas(std::make_unique<Canvas>(*this))
{
    addAndMakeVisible(*canvas);
    for (auto* button : { &zoomIn, &zoomOut, &fitButton, &expandButton, &branchButton, &technicalButton }) addAndMakeVisible(button);
    addAndMakeVisible(zoomLabel); addAndMakeVisible(detailText);
    detailText.setMultiLine(true); detailText.setReadOnly(true); detailText.setScrollbarsShown(true);
    detailText.setFont(juce::FontOptions(15));
    detailText.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff202226));
    detailText.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe4eaf0));
    zoomIn.onClick = [this] { zoomAt(1.2f, canvas->getLocalBounds().getCentre().toFloat()); };
    zoomOut.onClick = [this] { zoomAt(1 / 1.2f, canvas->getLocalBounds().getCentre().toFloat()); };
    fitButton.onClick = [this] { fit(); }; expandButton.onClick = [this] { expandAll(); };
    branchButton.onClick = [this] { toggleBranch(selectedId); };
    technicalButton.onClick = [this] { technicalShown = ! technicalShown; updateDetails(); };
}
ProvenanceGraphView::~ProvenanceGraphView() = default;
void ProvenanceGraphView::setHistory(const CredentialHistoryNode& history)
{
    graph = makeProvenanceGraph(history); collapsed.clear();
    for (const auto& child : graph.children) if (! child.children.empty()) collapsed.insert(child.id);
    selectedId = graph.id; technicalShown = false; refreshLayout(true); updateDetails();
}
void ProvenanceGraphView::refreshLayout(bool shouldFit)
{
    nodes = layoutProvenanceGraph(graph, collapsed);
    if (shouldFit) frame(true); else canvas->repaint();
}
void ProvenanceGraphView::fit()
{ frame(false); }
void ProvenanceGraphView::frame(bool readable)
{
    juce::Rectangle<float> bounds;
    for (const auto& node : nodes) bounds = bounds.getUnion(node.bounds);
    camera.fit(bounds.expanded(26), canvas->getLocalBounds().toFloat().reduced(12));
    if (readable && camera.zoom < 0.85f)
        camera.zoomAt(0.85f / camera.zoom, canvas->getLocalBounds().getCentre().toFloat());
    zoomLabel.setText(juce::String(juce::roundToInt(camera.zoom * 100)) + "%", juce::dontSendNotification);
    canvas->repaint();
}
void ProvenanceGraphView::expandAll() { collapsed.clear(); refreshLayout(true); }
void ProvenanceGraphView::toggleBranch(const juce::String& id)
{
    if (id.isEmpty()) return;
    const auto old = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) { return node.node->id == id; });
    if (old == nodes.end() || old->node->children.empty()) return;
    const auto anchor = camera.toScreen(old->bounds.getCentre());
    if (! collapsed.erase(id)) collapsed.insert(id);
    nodes = layoutProvenanceGraph(graph, collapsed);
    const auto current = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) { return node.node->id == id; });
    camera.translation = anchor - current->bounds.getCentre() * camera.zoom;
    selectedId = id; updateDetails(); canvas->repaint();
}
const ProvenanceGraphNode* ProvenanceGraphView::selected() const
{
    const auto found = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) { return node.node->id == selectedId; });
    return found == nodes.end() ? nullptr : found->node;
}
void ProvenanceGraphView::selectNode(const juce::String& id)
{ selectedId = id; technicalShown = false; updateDetails(); canvas->repaint(); }
void ProvenanceGraphView::updateDetails()
{
    const auto* node = selected();
    detailText.setText(node ? node->details + (technicalShown ? "\n\nTechnical evidence:\n" + node->technical : juce::String())
                             : "Select a node in the provenance graph.", false);
    technicalButton.setButtonText(technicalShown ? "Hide Technical Details" : "Technical Details");
    branchButton.setEnabled(node && ! node->children.empty());
}
void ProvenanceGraphView::zoomAt(float factor, juce::Point<float> anchor)
{
    camera.zoomAt(factor, anchor);
    zoomLabel.setText(juce::String(juce::roundToInt(camera.zoom * 100)) + "%", juce::dontSendNotification);
    canvas->repaint();
}
void ProvenanceGraphView::resized()
{
    auto area = getLocalBounds(); auto tools = area.removeFromTop(34);
    zoomOut.setBounds(tools.removeFromLeft(38).reduced(2)); zoomIn.setBounds(tools.removeFromLeft(38).reduced(2));
    fitButton.setBounds(tools.removeFromLeft(54).reduced(2)); expandButton.setBounds(tools.removeFromLeft(100).reduced(2));
    zoomLabel.setBounds(tools.removeFromLeft(62));
    area.removeFromBottom(26);
    sideBounds = area.removeFromRight(juce::jlimit(260, 320, getWidth() / 4)); area.removeFromRight(12);
    canvas->setBounds(area);
    auto side = sideBounds.reduced(8); side.removeFromTop(24);
    technicalButton.setBounds(side.removeFromBottom(30)); side.removeFromBottom(6);
    branchButton.setBounds(side.removeFromBottom(30)); side.removeFromBottom(8); detailText.setBounds(side);
    frame(true);
}
void ProvenanceGraphView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff17191c)); g.setColour(juce::Colour(0xffe0e3e8));
    g.setFont(juce::FontOptions(12, juce::Font::bold));
    g.drawText("Selected contribution", sideBounds.reduced(8).withHeight(20), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(12));
    g.drawText("Drag / scroll: pan  |  Pinch / Cmd-scroll: zoom  |  +/-: zoom  |  Fit: overview  |  Click a card for details",
               getLocalBounds().removeFromBottom(24).reduced(8, 0), juce::Justification::centredLeft);
}
}
