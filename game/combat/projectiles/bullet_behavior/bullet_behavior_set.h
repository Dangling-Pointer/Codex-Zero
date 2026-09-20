#pragma once

#include "bullet_behavior.h"

#include <memory>
#include <vector>

class BulletBehaviorSet
{
public:
    void add(std::unique_ptr<BulletBehavior> behavior);

    void on_fire(BulletBehaviorContext &context);

    void on_update(BulletBehaviorContext &context);

    [[nodiscard]] bool collision_handled(BulletBehaviorContext &context);

    [[nodiscard]] bool entity_collision_handled(BulletBehaviorContext &context);

    void replay_collision_behaviors_except(BulletBehaviorContext &context, BulletBehavior *skip_behavior);

    void on_death(BulletBehaviorContext &context);

private:
    std::vector<std::unique_ptr<BulletBehavior>> _behaviors;
};