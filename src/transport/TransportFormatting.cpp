#include "TransportFormatting.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace c2paseq::transport
{
double sanitisePosition(double positionSeconds) noexcept
{
    if (! std::isfinite(positionSeconds))
        return 0.0;

    return std::clamp(positionSeconds, 0.0, timelineEndSeconds);
}

std::string formatPosition(double positionSeconds)
{
    const auto totalMilliseconds = static_cast<long long>(
        std::llround(sanitisePosition(positionSeconds) * 1000.0));
    const auto minutes = totalMilliseconds / 60000;
    const auto seconds = (totalMilliseconds / 1000) % 60;
    const auto milliseconds = totalMilliseconds % 1000;

    std::ostringstream result;
    result << std::setfill('0') << std::setw(2) << minutes
           << ':' << std::setw(2) << seconds
           << '.' << std::setw(3) << milliseconds;
    return result.str();
}
}
