#include "PlacesStore.h"

namespace c2paseq
{
namespace
{
juce::File defaultPlacesFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("C2PA Creative Sequencer")
        .getChildFile("places.json");
}
}

PlacesStore::PlacesStore() : PlacesStore(defaultPlacesFile()) {}

PlacesStore::PlacesStore(juce::File storageFile) : file(std::move(storageFile))
{
    load();
}

const juce::Array<juce::File>& PlacesStore::folders() const noexcept { return roots; }

juce::Result PlacesStore::addFolder(const juce::File& folder)
{
    if (! folder.isDirectory())
        return juce::Result::fail("Selected place is not a folder");
    const auto canonical = folder.getFullPathName();
    for (const auto& existing : roots)
        if (existing.getFullPathName() == canonical)
            return juce::Result::ok();
    roots.add(folder);
    return save();
}

juce::Result PlacesStore::removeFolder(const juce::File& folder)
{
    for (int index = roots.size(); --index >= 0;)
        if (roots[index].getFullPathName() == folder.getFullPathName())
            roots.remove(index);
    return save();
}

void PlacesStore::load()
{
    if (! file.existsAsFile())
        return;
    const auto parsed = juce::JSON::parse(file);
    if (auto* values = parsed.getArray())
        for (const auto& value : *values)
            if (value.isString())
                roots.add(juce::File(value.toString()));
}

juce::Result PlacesStore::save() const
{
    if (! file.getParentDirectory().createDirectory())
        return juce::Result::fail("Could not create Places settings folder");
    juce::Array<juce::var> values;
    for (const auto& root : roots)
        values.add(root.getFullPathName());
    juce::TemporaryFile temporary(file);
    if (! temporary.getFile().replaceWithText(juce::JSON::toString(values, true))
        || ! temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not save Places folders");
    return juce::Result::ok();
}
}
