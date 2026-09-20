#pragma once

#include "../../../engine/core/geometry/vector2.h"

enum class SpreadStyle
{
    Uniform,
    Circular,
    Random
};

enum class ShotStyle
{
    Simultaneous,
    Sequential,
    ReverseSequential
};

struct WandAttributes
{
    // Need to implement these two
    float cooldown_seconds;
    float mana_cost;

    int bullet_count = 1;

    SpreadStyle spread_style = SpreadStyle::Uniform;
    float spread_degrees = 180.0;

    ShotStyle shot_style = ShotStyle::Simultaneous;
    float first_shot_delay = 0.0f;
    float shot_delay_sec = 0.1f;

    float spawn_distance = 32.0f;
};