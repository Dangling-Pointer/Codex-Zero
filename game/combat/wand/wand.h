#pragma once

#include "wand_types.h"
#include "../projectiles/bullet_types.h"
#include "../shot_descriptor.h"

#include "../rune/rune_line.h"

#include <cstdlib>
#include <vector>

// TODO wand randomness run through engine

class Wand
{
public:
    Wand();

    [[nodiscard]] std::vector<ShotDescriptor> attack(const elysia::core::Vector2 &direction);

private:
    [[nodiscard]] ShotDescriptor make_shot(const RuneLoadout &loadout, const elysia::core::Vector2 &direction,
                                           int index, float delay_offset);

    int append_weapon_shots(const RuneWeaponNode &weapon_node, const elysia::core::Vector2 &direction,
                            float delay_offset, std::vector<ShotDescriptor> &out_shots);

    float calculate_bullet_angle(const WandAttributes &wand_attributes, int index);
    float calc_uniform_spread_angle(const WandAttributes &wand_attributes, int num);
    float calc_circular_spread_angle(const WandAttributes &wand_attributes, int num);
    float calc_random_spread_angle(const WandAttributes &wand_attributes);

    float get_shot_delay(const WandAttributes &wand_attributes, int index);

    void seed_test_runes();

private:
    RuneLine _rune_line;
};
