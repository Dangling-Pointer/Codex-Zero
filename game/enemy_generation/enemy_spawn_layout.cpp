#include "enemy_spawn_layout.h"

/*
 * each purchase is assigned to an anchor in a round robin ish order
 * later purchases move across columns and then down rows within that anchor group
 * this keeps enemies structured and guarantees a new position for every purchase
 */

std::vector<elysia::core::Vector2> EnemySpawnLocator::find_group_anchors(
    elysia::core::Vector2 room_center) const
{
    return {
        room_center + elysia::core::Vector2{-128.0f, -64.0f},
        room_center + elysia::core::Vector2{128.0f, -64.0f}};
}

std::vector<EnemySpawn> EnemySpawnLayout::map(
    std::span<const EnemySpawn> purchases, elysia::core::Vector2 room_center) const
{
    const auto anchors = _locator.find_group_anchors(room_center);

    std::vector<EnemySpawn> mapping;
    mapping.reserve(purchases.size());
    for (std::size_t index = 0; index < purchases.size(); ++index)
    {
        const std::size_t group = index % anchors.size();
        const std::size_t group_slot = index / anchors.size();

        // TODO redo by enemy size
        const float horizontal_offset = static_cast<float>(group_slot % 5) * 24.0f - 48.0f;
        const float vertical_offset = static_cast<float>(group_slot / 5) * 24.0f;

        mapping.push_back(
            {purchases[index].definition,
             anchors[group] + elysia::core::Vector2{horizontal_offset, vertical_offset}});
    }
    return mapping;
}
