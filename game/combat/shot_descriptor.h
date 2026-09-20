#pragma once

#include "projectiles/bullet_types.h"

/*
ShotDescriptor is a per shot intention record, not a live projectile. It carries the bullet template,
the relative spawn offset, and the spawn delay. The room scene resolves the final world position when the
shot actually fires so delayed shots still spawn relative to the character’s current position.
*/
struct ShotDescriptor
{
    Bullet_Attributes bullet_attributes;

    // Bullet position position relative to character pos
    elysia::core::Vector2 spawn_offset;
    elysia::core::Vector2 shot_direction;

    float spawn_delay_sec;
};