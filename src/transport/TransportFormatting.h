#pragma once

#include <string>

namespace c2paseq::transport
{
inline constexpr double defaultBpm = 120.0;
inline constexpr double minimumBpm = 40.0;
inline constexpr double maximumBpm = 240.0;
inline constexpr double preferredSampleRate = 48000.0;
inline constexpr int preferredBlockSize = 512;
inline constexpr double timelineEndSeconds = 600.0;
inline constexpr double defaultLoopEndSeconds = 16.0;

[[nodiscard]] double sanitisePosition(double positionSeconds) noexcept;
[[nodiscard]] std::string formatPosition(double positionSeconds);
}
