#include "MediaLibrary.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>

namespace c2paseq
{
juce::Result MediaLibrary::copySourceIntoProject(Project& project,
                                                  const ProjectPaths& paths,
                                                  const juce::File& source,
                                                  MediaReference& reference)
{
    if (! source.existsAsFile())
        return juce::Result::fail("Source media does not exist");
    if (auto result = paths.createDirectories(); result.failed())
        return result;

    const auto sourceHash = juce::SHA256(source).toHexString();
    if (sourceHash.length() != 64)
        return juce::Result::fail("Could not hash source media");

    const auto existing = std::find_if(project.media.begin(), project.media.end(),
        [&sourceHash](const auto& item) { return item.sha256 == sourceHash; });
    if (existing != project.media.end())
    {
        const auto existingFile = paths.root().getChildFile(existing->relativePath);
        if (! existingFile.existsAsFile()
            || juce::SHA256(existingFile).toHexString() != sourceHash)
            return juce::Result::fail("Registered project media is missing or modified");
        reference = *existing;
        return juce::Result::ok();
    }

    auto destination = paths.mediaDirectory().getChildFile(source.getFileName());
    if (destination.exists())
    {
        const auto stem = source.getFileNameWithoutExtension();
        const auto extension = source.getFileExtension();
        destination = paths.mediaDirectory().getChildFile(
            stem + "-" + sourceHash.substring(0, 8) + extension);
    }

    juce::TemporaryFile temporary(destination);
    if (! source.copyFileTo(temporary.getFile()))
        return juce::Result::fail("Could not copy source media");
    if (juce::SHA256(temporary.getFile()).toHexString() != sourceHash)
        return juce::Result::fail("Copied media hash does not match source");
    if (! temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not commit copied media");

    reference.id = juce::Uuid().toString();
    reference.originalFileName = source.getFileName();
    reference.relativePath = "Media/" + destination.getFileName();
    reference.sha256 = sourceHash;
    reference.byteSize = source.getSize();
    project.media.push_back(reference);
    return juce::Result::ok();
}
}
