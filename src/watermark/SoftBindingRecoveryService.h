#pragma once

#include "SoftBindingStore.h"
#include "WatermarkService.h"
#include "provenance/ProvenanceService.h"

namespace c2paseq
{
class SoftBindingRecoveryService final
{
public:
    SoftBindingRecoveryService(WatermarkService&, SoftBindingStore&, ProvenanceService&);

    [[nodiscard]] juce::Result recover(const juce::File&, IngredientInfo&);

private:
    WatermarkService& watermark;
    SoftBindingStore& store;
    ProvenanceService& provenance;
};
}
