#include "ProvenanceGraph.h"
#include <cmath>

namespace c2paseq
{
namespace
{
juce::String sourceLabel(const juce::String& source)
{
    const auto term = source.fromLastOccurrenceOf("/", false, false);
    if (term == "digitalCapture") return "Captured audio (declared)";
    if (term == "digitalCreation") return "Digital creation (declared)";
    if (term == "trainedAlgorithmicMedia") return "Trained algorithmic media (declared)";
    // Preserve other vocabulary terms verbatim; do not invent a GenAI classifier.
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
    node.details += "\n\nEmbedded history is not independent validation of the ingredient audio.";
    return node;
}
float span(const ProvenanceGraphNode& node, const std::set<juce::String>& collapsed)
{
    if (node.children.empty() || collapsed.contains(node.id)) return 300;
    float width = -40;
    for (const auto& child : node.children) width += span(child, collapsed) + 40;
    return std::max(300.0f, width);
}
void place(const ProvenanceGraphNode& node, const std::set<juce::String>& collapsed,
           float x, float y, int parent, std::vector<ProvenanceGraphPlacement>& out)
{
    const auto width = span(node, collapsed);
    const auto index = static_cast<int>(out.size());
    out.push_back({ &node, { x + (width - 300) * 0.5f, y, 300, 188 }, parent });
    if (collapsed.contains(node.id)) return;
    for (const auto& child : node.children)
    { place(child, collapsed, x, y + 266, index, out); x += span(child, collapsed) + 40; }
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
        graphics.fillAll(juce::Colour(0xff11191f));
        juce::Graphics::ScopedSaveState saved(graphics);
        graphics.addTransform(juce::AffineTransform::scale(owner.camera.zoom)
            .translated(owner.camera.translation.x, owner.camera.translation.y));
        for (const auto& placed : owner.nodes)
        {
            if (placed.parent < 0) continue;
            const auto& parent = owner.nodes[static_cast<size_t>(placed.parent)];
            const auto start = parent.bounds.getBottomLeft() + juce::Point<float>(150, 0);
            const auto end = placed.bounds.getTopLeft() + juce::Point<float>(150, 0);
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
            graphics.setFont(juce::FontOptions(11));
            graphics.drawText(placed.node->relationship, juce::Rectangle<float>(end.x - 140, end.y - 24, 280, 18),
                              juce::Justification::centred);
        }
        for (const auto& placed : owner.nodes)
        {
            const auto& node = *placed.node;
            auto bounds = placed.bounds;
            graphics.setColour(juce::Colour(0xff25323c)); graphics.fillRoundedRectangle(bounds, 5);
            graphics.setColour(node.id == owner.selectedId ? juce::Colour(0xffe5c472)
                : node.hasManifest ? juce::Colour(0xff6ebacf) : juce::Colour(0xff81909c));
            graphics.drawRoundedRectangle(bounds, 5, node.id == owner.selectedId ? 2.5f : 1.2f);
            auto text = bounds.reduced(16, 12);
            graphics.setFont(juce::FontOptions(11, juce::Font::bold));
            graphics.drawText(placed.parent < 0 ? "SELECTED FILE" : node.relationship.isNotEmpty()
                ? "INGREDIENT - " + node.relationship : "EMBEDDED HISTORY", text.removeFromTop(20), juce::Justification::centredLeft);
            graphics.setColour(juce::Colours::white); graphics.setFont(juce::FontOptions(15, juce::Font::bold));
            graphics.drawFittedText(node.title, text.removeFromTop(40).toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
            graphics.setFont(juce::FontOptions(12)); graphics.setColour(juce::Colour(0xffc8d7e2));
            graphics.drawText(node.generator.isNotEmpty() ? node.generator
                : node.hasManifest ? "Claim generator not reported" : node.hasManifestReference
                    ? "Referenced manifest is not embedded" : node.credentialsAbsent
                        ? "No Content Credentials" : "Manifest / history unavailable",
                text.removeFromTop(21), juce::Justification::centredLeft);
            graphics.setColour(juce::Colour(0xffb4d9e0));
            graphics.drawText(node.sources.isEmpty() ? "Source type not declared" : node.sources[0],
                              text.removeFromTop(22), juce::Justification::centredLeft);
            if (node.instrument.isNotEmpty())
                graphics.drawText("Instrument: " + node.instrument, text.removeFromTop(20), juce::Justification::centredLeft);
            graphics.setColour(juce::Colour(0xffb8c3cb)); graphics.setFont(juce::FontOptions(11));
            graphics.drawText(node.sourceUnavailable ? node.credentialsAbsent
                ? "No CC - source audio unavailable" : "Source unavailable - history only"
                : node.hasManifest ? "Embedded credentials - see validation separately"
                    : node.hasManifestReference ? "Credentials referenced - not embedded"
                        : node.credentialsAbsent ? "No Content Credentials" : "Credentials unknown / not evaluated",
                text.removeFromTop(20), juce::Justification::centredLeft);
            if (! node.children.empty())
            {
                graphics.setColour(juce::Colour(0xffe5c472));
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
    detailText.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1c262e));
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
    if (shouldFit) fit(); else canvas->repaint();
}
void ProvenanceGraphView::fit()
{
    juce::Rectangle<float> bounds;
    for (const auto& node : nodes) bounds = bounds.getUnion(node.bounds);
    camera.fit(bounds.expanded(26), canvas->getLocalBounds().toFloat().reduced(12));
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
    fit();
}
void ProvenanceGraphView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff182127)); g.setColour(juce::Colour(0xffc4d4df));
    g.setFont(juce::FontOptions(12, juce::Font::bold));
    g.drawText("Selected contribution", sideBounds.reduced(8).withHeight(20), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(12));
    g.drawText("Drag / scroll: pan  |  Pinch / Cmd-scroll: zoom  |  +/-: zoom  |  Fit: overview  |  Click a card for details",
               getLocalBounds().removeFromBottom(24).reduced(8, 0), juce::Justification::centredLeft);
}
}
