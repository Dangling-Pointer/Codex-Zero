#pragma once
#include "damage_receiver.h"
#include "engine/core/scene_object.h"
#include "engine/gameplay/collision/gameplay_collision_runtime.h"
#include <functional>
#include <unordered_map>
class Character;
class Bullet;

class CombatSystem final : private elysia::gameplay::collision::GameplayCollisionListener,
                           private elysia::gameplay::collision::TeamRelationResolver
{
public:
    explicit CombatSystem(elysia::gameplay::collision::GameplayCollisionRuntime& runtime);
    ~CombatSystem();
    CombatSystem(const CombatSystem&) = delete;
    CombatSystem& operator=(const CombatSystem&) = delete;

    bool register_character(Character& character);
    bool register_bullet(Bullet& bullet);
    void remove(elysia::core::SceneObject& object);
    void clear();
    std::function<void(elysia::gameplay::collision::ActorId, const AttackInfo&, const DamageResult&)> on_damage;
private:
    void on_hit_overlap(const elysia::gameplay::collision::HitOverlapEvent& event) override;
    elysia::gameplay::collision::TeamRelation relation(
        elysia::gameplay::collision::TeamId source,
        elysia::gameplay::collision::TeamId target) const noexcept override;
    elysia::gameplay::collision::GameplayCollisionRuntime& _runtime;
    std::unordered_map<elysia::gameplay::collision::ActorId, Character*> _receivers;
    std::unordered_map<elysia::gameplay::collision::AttackInstanceId, Bullet*> _attacks;
};
