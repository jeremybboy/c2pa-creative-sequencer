#pragma once
#include "ProvenanceModel.h"
#include <array>

namespace c2paseq
{
enum class CredentialCheckState { pass, fail, warning, unknown, notApplicable };
struct CredentialCheck
{
    CredentialCheckState state = CredentialCheckState::unknown;
    juce::String title, summary, evidence;
};
struct CredentialHistoryNode
{
    juce::String title, subtitle, details;
    std::vector<CredentialHistoryNode> children;
};
// Presentation only: no SDK calls, signing, IO, inferred authorship or trust configuration.
struct CredentialViewModel
{
    juce::String title, badge, description, policy, details;
    std::array<CredentialCheck, 3> checks;
    CredentialHistoryNode history;
};
[[nodiscard]] CredentialViewModel makeCredentialViewModel(
    const IngredientInfo&, const juce::String& title,
    const std::vector<ContributingIngredient>& locallyInspected = {});
[[nodiscard]] CredentialViewModel makeUnrenderedMidiCredentialView(const juce::String& title);
}
