#include "RenderService.h"

#include "engine/ClipOcclusion.h"
#include "engine/TracktionAdapter.h"

#include <algorithm>

namespace c2paseq
{
juce::Result RenderService::createPlan(const Project& project,
                                       const ProjectPaths& paths,
                                       RenderPlan& plan)
{
    RenderPlan candidate;
    const auto anySolo = std::any_of(project.tracks.begin(), project.tracks.end(),
                                     [](const auto& track) { return track.soloed; });
    std::vector<juce::String> includedMedia;

    for (const auto& track : project.tracks)
    {
        if (track.muted || (anySolo && ! track.soloed))
            continue;

        for (const auto& segment : buildPlaybackClipSegments(track.clips))
        {
            const auto& clip = track.clips[segment.clipIndex];
            candidate.endSeconds = std::max(candidate.endSeconds,
                segment.startSeconds + segment.lengthSeconds);
            if (std::find(includedMedia.begin(), includedMedia.end(), clip.mediaId)
                != includedMedia.end())
                continue;

            const auto media = std::find_if(project.media.begin(), project.media.end(),
                [&](const auto& item) { return item.id == clip.mediaId; });
            if (media == project.media.end())
                return juce::Result::fail("An audible clip references missing project media");
            const auto file = paths.root().getChildFile(media->relativePath);
            if (! file.existsAsFile())
                return juce::Result::fail("Contributing source media is missing");
            includedMedia.push_back(media->id);
            candidate.ingredients.push_back({ media->id, media->originalFileName,
                                               media->sha256, file, media->provenance });
        }
    }

    if (candidate.endSeconds <= 0.0 || candidate.ingredients.empty())
        return juce::Result::fail("The project has no audible clips to export");
    plan = std::move(candidate);
    return juce::Result::ok();
}

juce::Result RenderService::render(TracktionAdapter& tracktion,
                                   const RenderPlan& plan,
                                   const juce::File& destination)
{
    if (! destination.hasFileExtension("wav"))
        return juce::Result::fail("Export destination must be a WAV file");
    return tracktion.renderWav(destination, plan.endSeconds);
}
}
