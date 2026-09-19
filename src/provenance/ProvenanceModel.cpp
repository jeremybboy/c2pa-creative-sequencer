#include "ProvenanceModel.h"

namespace c2paseq
{
SoftBindingClaim makeAudioWMarkClaim(const SoftBindingPayload& payload,
                                    std::uint64_t endMilliseconds)
{
    return { juce::String(audioWMarkAlgorithm.data()), SoftBindingType::watermark,
             std::vector<std::uint8_t>(payload.bytes.begin(), payload.bytes.end()),
             SoftBindingScope { 0, endMilliseconds } };
}

juce::String provenanceStatusId(ProvenanceStatus status)
{
    switch (status)
    {
        case ProvenanceStatus::valid: return "VALID";
        case ProvenanceStatus::presentWithValidationIssue:
            return "PRESENT_WITH_VALIDATION_ISSUE";
        case ProvenanceStatus::noCredentials: return "NO_CREDENTIALS";
        case ProvenanceStatus::unableToValidate: return "UNABLE_TO_VALIDATE";
    }
    return "UNABLE_TO_VALIDATE";
}

juce::String provenanceStatusLabel(ProvenanceStatus status)
{
    switch (status)
    {
        case ProvenanceStatus::valid: return "Content Credentials valid";
        case ProvenanceStatus::presentWithValidationIssue:
            return "Content Credentials present with validation issue";
        case ProvenanceStatus::noCredentials: return "No Content Credentials";
        case ProvenanceStatus::unableToValidate: return "Unable to validate Content Credentials";
    }
    return "Unable to validate Content Credentials";
}

ProvenanceStatus provenanceStatusFromId(const juce::String& id)
{
    if (id == "VALID") return ProvenanceStatus::valid;
    if (id == "PRESENT_WITH_VALIDATION_ISSUE")
        return ProvenanceStatus::presentWithValidationIssue;
    if (id == "NO_CREDENTIALS") return ProvenanceStatus::noCredentials;
    return ProvenanceStatus::unableToValidate;
}
}
