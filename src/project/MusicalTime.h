#pragma once

#include <cassert>

namespace c2paseq
{
struct BeatPosition final
{
    double beats = 0.0;
};

struct BeatDuration final
{
    double beats = 0.0;
};

// The project model stores MIDI positions in beats. This small boundary owns the
// current constant-tempo conversion so a future tempo map can replace it without
// changing the saved meaning of MIDI notes.
class MusicalTimeConverter final
{
public:
    explicit MusicalTimeConverter(double beatsPerMinute) noexcept
        : bpm(beatsPerMinute)
    {
        assert(bpm > 0.0);
    }

    [[nodiscard]] double toSeconds(BeatPosition position) const noexcept
    {
        return position.beats * secondsPerBeat();
    }

    [[nodiscard]] double toSeconds(BeatDuration duration) const noexcept
    {
        return duration.beats * secondsPerBeat();
    }

    [[nodiscard]] BeatPosition toBeatPosition(double seconds) const noexcept
    {
        return { seconds / secondsPerBeat() };
    }

private:
    [[nodiscard]] double secondsPerBeat() const noexcept { return 60.0 / bpm; }

    double bpm = 120.0;
};
}
