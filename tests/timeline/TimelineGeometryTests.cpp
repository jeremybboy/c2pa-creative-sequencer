#include "timeline/TimelineGeometry.h"
#include "ui/PlacesStore.h"

#include <cmath>
#include <iostream>

namespace
{
bool close(double a, double b) { return std::abs(a - b) < 0.000001; }
}

int main()
{
    c2paseq::TimelineGeometry geometry;
    geometry.bpm = 120.0;
    geometry.pixelsPerSecond = 100.0;
    geometry.scrollSeconds = 3.0;

    if (! close(geometry.beatSeconds(), 0.5) || ! close(geometry.barSeconds(), 2.0))
        return 1;
    if (! close(geometry.timeToX(5.0), 200.0)
        || ! close(geometry.xToTime(200.0), 5.0))
        return 2;
    if (! close(geometry.snapToBeat(2.26), 2.5))
        return 3;

    const auto anchorTime = geometry.xToTime(320.0);
    geometry.zoomAround(200.0, 320.0);
    if (! close(geometry.xToTime(320.0), anchorTime))
        return 4;

    const auto temporary = juce::File::getCurrentWorkingDirectory()
        .getNonexistentChildFile("c2paseq-places-test", {}, false);
    if (! temporary.createDirectory())
        return 5;
    const auto sampleFolder = temporary.getChildFile("Samples");
    if (! sampleFolder.createDirectory())
        return 6;
    const auto storeFile = temporary.getChildFile("places.json");
    {
        c2paseq::PlacesStore store(storeFile);
        if (store.addFolder(sampleFolder).failed() || store.folders().size() != 1)
            return 7;
        if (store.addFolder(sampleFolder).failed() || store.folders().size() != 1)
            return 8;
    }
    {
        c2paseq::PlacesStore restored(storeFile);
        if (restored.folders().size() != 1
            || restored.folders()[0].getFullPathName() != sampleFolder.getFullPathName())
            return 9;
        if (restored.removeFolder(sampleFolder).failed() || ! restored.folders().isEmpty())
            return 10;
    }
    temporary.deleteRecursively();

    std::cout << "timeline geometry and persistent Places passed\n";
    return 0;
}
