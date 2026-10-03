#include "combat_system.h"
#include "game/characters/character.h"
#include "projectiles/bullet.h"

// I kind of know what's happening, but not too much - Akil

using namespace elysia::gameplay::collision;
CombatSystem::CombatSystem(GameplayCollisionRuntime& runtime) : _runtime(runtime)
{
    // Listen for gameplay hit-overlap events and provide team relation queries
    (void)_runtime.add_listener(*this);
    _runtime.set_team_relation_resolver(this);
}

CombatSystem::~CombatSystem()
{
    // Release all combat bindings before detaching from the collision runtime
    clear();
    (void)_runtime.remove_listener(*this);
    _runtime.set_team_relation_resolver(nullptr);
}

TeamRelation CombatSystem::relation(TeamId source, TeamId target) const noexcept
{
    if (source == InvalidTeamId || target == InvalidTeamId)
        return TeamRelation::Neutral;

    if (source == teams::Neutral && (target == teams::Player || target == teams::Enemy))
        // Neutral attacks are treated as hostile toward both gameplay factions
        return TeamRelation::Hostile;

    return source == target ? TeamRelation::Friendly : TeamRelation::Hostile;
}

bool CombatSystem::register_character(Character& character)
{
    if (character.is_destroyed() || character.is_dead() || !_runtime.bind_actor(character.collision_rig()))
        return false;

    // Temporary receiver lookup; remove after the upcoming engine lifecycle refactor
    try {
        _receivers.emplace(character.actor_id(), &character);
    }
    catch (...) {
        (void)_runtime.unbind_actor(character.actor_id());
        throw;
    }
    return true;
}

bool CombatSystem::register_bullet(Bullet& bullet)
{
    using namespace game::collision::categories;
    const auto category = bullet.get_bullet_attributes()->collision_category;

    // Derive the attack team from the bullet collision category
    const TeamId team =
        category == PlayerAttack ? teams::Player :
        category == EnemyAttack ? teams::Enemy :
        teams::Neutral;

    if (!bullet.instigator())
        return false;

    // Register the bullet collider as a gameplay hit box tied to this attack instance
    HitBoxBinding hit{{bullet.physics_collider(0), InvalidActorId, team, ColliderRole::HitBox},
                      bullet.instigator(), bullet.attack_instance(), 1};

    if (bullet.is_destroyed() || !_runtime.bind_hit_box(hit))
        return false;

    try { _attacks.emplace(bullet.attack_instance(), &bullet); }
    catch (...) { _runtime.end_attack_instance(bullet.attack_instance()); throw; }

    return true;
}

void CombatSystem::remove(elysia::core::SceneObject& object)
{
    if (auto* c = dynamic_cast<Character*>(&object))
    {
        (void)_runtime.unbind_actor(c->actor_id());
        _receivers.erase(c->actor_id());
    }
    if (auto* b = dynamic_cast<Bullet*>(&object))
    {
        _runtime.end_attack_instance(b->attack_instance());
        _attacks.erase(b->attack_instance());
    }
}

void CombatSystem::clear()
{
    for (const auto& [id, bullet] : _attacks)
        _runtime.end_attack_instance(id);

    for (const auto& [id, receiver] : _receivers)
        (void)_runtime.unbind_actor(id);

    _attacks.clear();
    _receivers.clear();
}

void CombatSystem::on_hit_overlap(const HitOverlapEvent& event)
{
    // Resolve the gameplay objects associated with the hit box and hurt box
    const auto attack = _attacks.find(event.hit_box.attack_instance);
    const auto target = _receivers.find(event.hurt_box.owner);


    if (attack == _attacks.end() || target == _receivers.end()) 
        return;

    Bullet& bullet = *attack->second;
    Character& receiver = *target->second;

    // Ignore stale or inactive scene objects that may still have queued overlap eventsa
    if (bullet.is_destroyed() || !bullet.is_active() || receiver.is_destroyed() || !receiver.is_active())
        return;

    // Convert projectile state into the combat payload consumed by the receiver
    const AttackInfo info{ bullet.instigator(),
    bullet.attack_instance(),bullet.get_bullet_attributes()->damage };

    const auto result = receiver.receive_attack(info);

    if (result.accepted && result.health_lost > 0)
    {
        // Reconstruct a collision event so the projectile can run its normal
        // entity-hit response after combat damage has been accepted
        elysia::physics::CollisionEvent collision;
        collision.phase = event.phase;
        collision.contact.pair = event.overlap.pair;
        collision.contact.manifold = event.overlap.manifold;
        collision.contact.response = elysia::physics::CollisionResponse::Overlap;

        // Destroy the projectile only if its entity-collision handler consumes the hit
        if (bullet.on_entity_collision(collision))
            bullet.destroy();
    }

    if (on_damage)
        on_damage(receiver.actor_id(), info, result);
}
