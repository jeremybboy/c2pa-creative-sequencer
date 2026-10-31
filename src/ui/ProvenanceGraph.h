#pragma once
#include "provenance/CredentialViewModel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <set>

namespace c2paseq
{
struct ProvenanceGraphNode
{
    juce::String id, title, generator, relationship, availability, details, technical;
    juce::StringArray sources, actions;
    juce::String instrument;
    bool sourceUnavailable = false;
    bool hasManifest = false;
    bool hasManifestReference = false;
    bool credentialsAbsent = false;
    std::vector<ProvenanceGraphNode> children;
};
[[nodiscard]] ProvenanceGraphNode makeProvenanceGraph(const CredentialHistoryNode&);
struct ProvenanceGraphPlacement
{
    const ProvenanceGraphNode* node = nullptr;
    juce::Rectangle<float> bounds;
    int parent = -1;
};
[[nodiscard]] std::vector<ProvenanceGraphPlacement> layoutProvenanceGraph(
    const ProvenanceGraphNode&, const std::set<juce::String>& collapsed);

// Graph camera is independent from project/timeline zoom and musical transport.
struct ProvenanceGraphCamera
{
    float zoom = 1.0f;
    juce::Point<float> translation;
    juce::Point<float> toScreen(juce::Point<float> p) const { return p * zoom + translation; }
    juce::Point<float> toWorld(juce::Point<float> p) const { return (p - translation) / zoom; }
    void zoomAt(float factor, juce::Point<float> anchor);
    void fit(juce::Rectangle<float> content, juce::Rectangle<float> viewport);
};

class ProvenanceGraphView final : public juce::Component
{
public:
    ProvenanceGraphView();
    ~ProvenanceGraphView() override;
    void setHistory(const CredentialHistoryNode&);
    void resized() override;
    void paint(juce::Graphics&) override;
    void fit();
    void expandAll();
    void toggleBranch(const juce::String& id);
    void selectNode(const juce::String& id);
    void zoomAt(float factor, juce::Point<float> anchor);
    [[nodiscard]] const auto& placements() const { return nodes; }
    [[nodiscard]] const auto& graphCamera() const { return camera; }
private:
    class Canvas;
    void refreshLayout(bool shouldFit);
    void frame(bool readable);
    void updateDetails();
    const ProvenanceGraphNode* selected() const;
    std::unique_ptr<Canvas> canvas;
    ProvenanceGraphNode graph;
    std::vector<ProvenanceGraphPlacement> nodes;
    ProvenanceGraphCamera camera;
    std::set<juce::String> collapsed;
    juce::String selectedId;
    bool technicalShown = false;
    juce::TextButton zoomIn { "+" }, zoomOut { "-" }, fitButton { "Fit" }, expandButton { "Expand All" };
    juce::TextButton branchButton { "Expand / Collapse Branch" }, technicalButton { "Technical Details" };
    juce::Label zoomLabel;
    juce::TextEditor detailText;
    juce::Rectangle<int> sideBounds;
};
}
