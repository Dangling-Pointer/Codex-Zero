#include "bullet_behavior_set.h"
#include "bullet_behavior_context.h"
#include "../bullet.h"

void BulletBehaviorSet::add(std::unique_ptr<BulletBehavior> behavior)
{
    _behaviors.push_back(std::move(behavior));
}

void BulletBehaviorSet::on_fire(BulletBehaviorContext &context)
{
    for (auto &behavior : _behaviors)
        behavior->on_fire(context);
}

void BulletBehaviorSet::on_update(BulletBehaviorContext &context)
{
    for (auto &behavior : _behaviors)
        behavior->on_update(context);
}

bool BulletBehaviorSet::collision_handled(BulletBehaviorContext &context)
{
    for (auto &behavior : _behaviors)
    {
        if (behavior->on_collision(context))
            return true;
    }
    return false;
}

bool BulletBehaviorSet::entity_collision_handled(BulletBehaviorContext &context)
{
    for (auto &behavior : _behaviors)
    {
        if (behavior->on_entity_collision(context))
            return true;
    }
    return false;
}

void BulletBehaviorSet::on_death(BulletBehaviorContext &context)
{
    for (auto &behavior : _behaviors)
        behavior->on_death(context);
}

/*
Plays collision behaviors for cases where a collision behavior triggers the effects of
other collision behaviors without re triggering itself.
Unlike on collision does not stop when behavior handles collision
*/
void BulletBehaviorSet::replay_collision_behaviors_except(BulletBehaviorContext &context,
                                                          BulletBehavior *skip_behavior)
{
    for (auto &behavior : _behaviors)
    {
        if (behavior.get() == skip_behavior)
            continue;

        behavior->on_collision(context);
    }
}
