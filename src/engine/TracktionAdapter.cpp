#include "TracktionAdapter.h"

#include "app/AppInfo.h"

namespace c2paseq
{
TracktionAdapter::TracktionAdapter()
    : engine(appInfo::name.data())
{
    // Engine construction performs Tracktion's synchronous subsystem and
    // device-manager initialisation. Reaching this body means it completed.
    initialised = true;
}

bool TracktionAdapter::isInitialised() const noexcept
{
    return initialised;
}

juce::String TracktionAdapter::audioDeviceDescription() const
{
    const auto& deviceManager = engine.getDeviceManager().deviceManager;

    if (const auto* device = deviceManager.getCurrentAudioDevice())
        return "Audio engine ready — " + device->getName();

    return "Audio engine ready — no output device selected";
}
}
