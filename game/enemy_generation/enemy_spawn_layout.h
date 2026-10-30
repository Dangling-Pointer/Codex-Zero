#pragma once

#include "enemy_generator.h"

#include <span>
#include <vector>

/*
 decides where spawned enemies should appear in the room
 the locator (not implemented yet) supplies safe group anchors while the layout supplies
 local formation offsets
 */

// TODO this is just a stub, responsibility should be replaced by real locator
class EnemySpawnLocator
{
public:
    // TODO: currently hard coded change to query of room locator
    [[nodiscard]] std::vector<elysia::core::Vector2> find_group_anchors(
        elysia::core::Vector2 room_center) const;
};

class EnemySpawnLayout
{
public:
    // create unique positions arranged in small groups around the room
    [[nodiscard]] std::vector<EnemySpawn> map(
        std::span<const EnemySpawn> purchases,
        elysia::core::Vector2 room_center) const;

private:
    EnemySpawnLocator _locator;
};
