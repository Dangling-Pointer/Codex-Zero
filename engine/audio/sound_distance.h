#pragma once

namespace elysia::audio
{
// World distances in EU. Callers must supply a valid configuration explicitly.
struct SoundDistanceConfig
{
    double reference_distance = 0;
    double cutoff_distance = 0;
    double release_distance = 0;
    double exponent = 1;
};

[[nodiscard]] bool valid_sound_distance(const SoundDistanceConfig& config) noexcept;
// Invalid distances/configurations produce silence.
[[nodiscard]] double sound_distance_gain(double distance,const SoundDistanceConfig& config) noexcept;
}
