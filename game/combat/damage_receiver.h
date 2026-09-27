#pragma once
#include "engine/gameplay/collision/gameplay_collision_types.h"

struct AttackInfo
{
    // Actor responsible for initiating this attack
    elysia::gameplay::collision::ActorId instigator = 0;
    // Unique instance ID used to identify and track this specific attack
    elysia::gameplay::collision::AttackInstanceId instance = 0;
    float damage = 0;
};
struct DamageResult
{
    bool accepted = false;
    float health_lost = 0;
    bool killed = false;
};
class CombatReceiver
{
public:
    virtual ~CombatReceiver() = default;
    virtual DamageResult receive_attack(const AttackInfo& attack) = 0;
};
