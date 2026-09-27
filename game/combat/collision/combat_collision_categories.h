#pragma once

#include "engine/physics/collision/collider.h"

namespace game::collision::categories
{
using CollisionBits = elysia::physics::CollisionBits;
// All kinds of gameplay-related collision bits.
inline constexpr CollisionBits World = 1u;
inline constexpr CollisionBits Player = 1u << 1;
inline constexpr CollisionBits Enemy = 1u << 2;
inline constexpr CollisionBits PlayerAttack = 1u << 3;
inline constexpr CollisionBits EnemyAttack = 1u << 4;
inline constexpr CollisionBits PlayerHurt = 1u << 6;
inline constexpr CollisionBits EnemyHurt = 1u << 7;
inline constexpr CollisionBits NeutralAttack = 1u << 5;
}