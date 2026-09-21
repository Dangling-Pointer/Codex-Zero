#include "rune_line.h"

#include "behavior_rune.h"
#include "stat_rune.h"
#include "weapon_rune.h"

#include <algorithm>

namespace
{
    // Consumption windows use an exclusive end index. Clamping here keeps
    // every caller from having to repeat the same end-of-line check.
    int bounded_scope_end(int weapon_slot_index,
                          int consumed_runes,
                          int limit) noexcept
    {
        // Treat invalid negative consumption as an empty window. Use a
        // subtraction-based comparison before adding to avoid signed integer
        // overflow when data comes from a custom WeaponRune.
        const int safe_consumed_runes = std::max(0, consumed_runes);
        const int first_consumed_slot = weapon_slot_index + 1;
        if (safe_consumed_runes > limit - first_consumed_slot)
        {
            return limit;
        }

        return std::min(first_consumed_slot + safe_consumed_runes, limit);
    }

    // A RuneType value is not enough to safely call weapon-specific methods.
    // Keep the RTTI check in one place so malformed/custom rune subclasses are
    // ignored instead of being treated as weapons.
    const WeaponRune *as_weapon(const std::shared_ptr<const Rune> &rune) noexcept
    {
        return rune ? dynamic_cast<const WeaponRune *>(rune.get()) : nullptr;
    }

    // Apply only runes that modify the current weapon. Weapon runes are
    // handled separately because they create a new evaluation node.
    void apply_non_weapon_rune(const Rune &rune, RuneLoadout &loadout)
    {
        switch (rune.type())
        {
        case RuneType::Stat:
            if (const auto *stat_rune = dynamic_cast<const StatRune *>(&rune))
            {
                stat_rune->apply_stat(loadout);
            }
            break;

        case RuneType::Behavior:
            if (const auto *behavior_rune = dynamic_cast<const BehaviorRune *>(&rune))
            {
                behavior_rune->apply_weapon(loadout);
            }
            break;

        case RuneType::Weapon:
            break;
        }
    }

    // Merge child modifiers into the parent without overwriting the parent's base state.
    void merge_loadout(RuneLoadout &target, const RuneLoadout &source)
    {
        // AddModifiers is intentionally conservative for attributes: a child
        // weapon's zero/default fields must not erase the parent's values, and
        // the stronger value wins for the attributes currently represented as
        // additive modifiers.
        target.wand_attributes.bullet_count = std::max(
            target.wand_attributes.bullet_count, source.wand_attributes.bullet_count);
        target.wand_attributes.spawn_distance = std::max(
            target.wand_attributes.spawn_distance, source.wand_attributes.spawn_distance);
        target.wand_attributes.shot_delay_sec = std::max(
            target.wand_attributes.shot_delay_sec, source.wand_attributes.shot_delay_sec);
        target.bullet_attributes.bullet_speed = std::max(
            target.bullet_attributes.bullet_speed, source.bullet_attributes.bullet_speed);
        target.bullet_attributes.max_age = std::max(
            target.bullet_attributes.max_age, source.bullet_attributes.max_age);
        target.bullet_attributes.damage += source.bullet_attributes.damage;
        target.bullet_behavior_appenders.insert(
            target.bullet_behavior_appenders.end(),
            source.bullet_behavior_appenders.begin(),
            source.bullet_behavior_appenders.end());
    }
}

RuneLine::RuneLine(int starting_slot_count)
    : _runes(starting_slot_count > 0 ? starting_slot_count : 1)
{
}

int RuneLine::slot_count() const noexcept
{
    return static_cast<int>(_runes.size());
}

bool RuneLine::in_bounds(int slot_index) const noexcept
{
    return slot_index >= 0 && slot_index < slot_count();
}

bool RuneLine::set_rune(int slot_index, std::shared_ptr<const Rune> rune)
{
    if (slot_index < 0)
    {
        return false;
    }

    if (slot_index >= slot_count())
    {
        _runes.resize(slot_index + 1, nullptr);
    }

    _runes[slot_index] = std::move(rune);

    return true;
}

std::shared_ptr<const Rune> RuneLine::rune_at(int slot_index) const noexcept
{
    if (!in_bounds(slot_index))
    {
        return nullptr;
    }

    return _runes[slot_index];
}

RuneLoadout RuneLine::evaluate() const
{
    RuneLoadout loadout;
    std::vector<RuneWeaponNode> weapons = evaluate_weapons();

    // The first weapon is the primary weapon in the line. Nested weapons are
    // represented by its children and are intentionally not merged here.
    if (!weapons.empty())
    {
        loadout = weapons.front().loadout;
    }

    return loadout;
}

std::vector<RuneWeaponNode> RuneLine::evaluate_weapons() const
{
    std::vector<RuneWeaponNode> weapons;
    int slot_index = 0;

    // Each iteration starts at the first slot not owned by the previous
    // weapon's subtree. This is what prevents consumed runes from being
    // evaluated twice.
    while (slot_index < slot_count())
    {
        std::shared_ptr<const Rune> rune = rune_at(slot_index);
        if (!rune)
        {
            ++slot_index;
            continue;
        }

        const WeaponRune *weapon_rune = as_weapon(rune);
        if (!weapon_rune)
        {
            ++slot_index;
            continue;
        }

        const WeaponConsumptionData consumption = weapon_rune->consumption();
        // The end is exclusive: a weapon at slot 0 consuming three runes owns
        // slots 0 through 3, so the next top-level scan starts at slot 4.
        const int scope_end_index = bounded_scope_end(
            slot_index, consumption.consumed_runes, slot_count());

        int subtree_end_index = scope_end_index;
        weapons.push_back(build_weapon_node(slot_index, scope_end_index, subtree_end_index));
        slot_index = subtree_end_index;
    }

    return weapons;
}

RuneWeaponNode RuneLine::build_weapon_node(int weapon_slot_index, int scope_end_index, int &subtree_end_index) const
{
    RuneWeaponNode node;
    node.slot_index = weapon_slot_index;

    const std::shared_ptr<const Rune> weapon_slot = rune_at(weapon_slot_index);
    const WeaponRune *weapon_rune = as_weapon(weapon_slot);
    if (!weapon_rune)
    {
        return node;
    }

    node.consumption = weapon_rune->consumption();
    weapon_rune->apply_weapon(node.loadout);

    // The parent owns its entire declared window, including empty slots and
    // stat/behavior runes. Initializing to scope_end_index is important: if we
    // started at weapon_slot_index + 1, another weapon inside this window
    // would be incorrectly emitted as a second top-level weapon.
    subtree_end_index = scope_end_index;

    int slot_index = weapon_slot_index + 1;
    while (slot_index < scope_end_index && slot_index < slot_count())
    {
        std::shared_ptr<const Rune> rune = rune_at(slot_index);
        if (!rune)
        {
            ++slot_index;
            continue;
        }

        if (const WeaponRune *child_weapon = as_weapon(rune))
        {
            const WeaponConsumptionData child_consumption = child_weapon->consumption();

            // A nested weapon starts a new window. Its window is bounded by
            // the actual rune line, not by the parent's old window; otherwise
            // a child near the end of the parent window would be truncated.
            const int child_scope_end = bounded_scope_end(
                slot_index, child_consumption.consumed_runes, slot_count());

            int child_subtree_end_index = child_scope_end;
            RuneWeaponNode child_node = build_weapon_node(
                slot_index, child_scope_end, child_subtree_end_index);

            subtree_end_index = child_subtree_end_index;

            if (node.consumption.consume_type == WeaponConsumeType::AddModifiers)
            {
                merge_loadout(node.loadout, child_node.loadout);
                node.children.insert(node.children.end(),
                                     child_node.children.begin(),
                                     child_node.children.end());
            }
            else
            {
                node.children.push_back(std::move(child_node));
            }

            // The parent window closes at the nested weapon. Any runes after
            // this point belong to the child, so the parent must not resume
            // applying them after recursion returns.
            return node;
        }
        else
        {
            apply_non_weapon_rune(*rune, node.loadout);
        }

        ++slot_index;
    }

    return node;
}
