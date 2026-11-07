#include "sound_distance.h"
#include <cmath>

namespace elysia::audio
{
bool valid_sound_distance(const SoundDistanceConfig& c) noexcept
{
    return std::isfinite(c.reference_distance) && c.reference_distance > 0 &&
        std::isfinite(c.cutoff_distance) && c.cutoff_distance >= c.reference_distance &&
        std::isfinite(c.release_distance) && c.release_distance >= c.cutoff_distance &&
        std::isfinite(c.exponent) && c.exponent > 0;
}

double sound_distance_gain(double distance,const SoundDistanceConfig& c) noexcept
{
    if (!valid_sound_distance(c) || !std::isfinite(distance) || distance < 0 || distance > c.cutoff_distance)
        return 0;
    return distance <= c.reference_distance ? 1 : std::pow(c.reference_distance / distance,c.exponent);
}
}
