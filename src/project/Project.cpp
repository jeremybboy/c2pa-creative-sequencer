#include "Project.h"

#include "app/AppInfo.h"

namespace c2paseq
{
juce::String trackTypeId(TrackType type)
{
    return type == TrackType::midi ? "midi" : "audio";
}

bool trackTypeFromId(const juce::String& id, TrackType& type)
{
    if (id == "audio")
    {
        type = TrackType::audio;
        return true;
    }
    if (id == "midi")
    {
        type = TrackType::midi;
        return true;
    }
    return false;
}

Project Project::create(juce::String projectName)
{
    const auto now = juce::Time::getCurrentTime().toISO8601(true);

    Project project;
    project.id = juce::Uuid().toString();
    project.name = projectName.trim().isNotEmpty() ? projectName.trim() : "Untitled Project";
    project.createdAt = now;
    project.modifiedAt = now;
    project.applicationVersion = appInfo::version.data();
    return project;
}
}
