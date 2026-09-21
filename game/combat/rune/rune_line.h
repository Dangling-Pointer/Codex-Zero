#pragma once

#include "rune.h"

#include <memory>
#include <vector>

/*
The rune line is a linear list of slots. A weapon consumes a bounded window of
slots after itself. Stat and behavior runes in that window modify the weapon;
a nested weapon starts its own window and cannot be consumed a second time.

FireNested keeps the child as a separate node so the firing stage can create
the additional branch. AddModifiers folds the child loadout into its parent
without creating another shot.
*/

class RuneLine
{
public:
    explicit RuneLine(int starting_slot_count = 1);

    // The line always has at least one addressable slot, even when constructed
    // with a non-positive starting size.
    [[nodiscard]] int slot_count() const noexcept;
    [[nodiscard]] bool in_bounds(int slot_index) const noexcept;

    // Setting a slot beyond the current end grows the line and leaves the
    // intervening slots empty. Passing nullptr clears a slot.
    [[nodiscard]] bool set_rune(int slot_index, std::shared_ptr<const Rune> rune);
    [[nodiscard]] std::shared_ptr<const Rune> rune_at(int slot_index) const noexcept;

    // Returns the primary weapon's final loadout. Use evaluate_weapons() when
    // the caller needs the nested weapon tree and its per-node metadata.
    [[nodiscard]] RuneLoadout evaluate() const;

    // Returns one node for each top-level weapon. A node owns its consumed
    // slot window and stores nested weapons in children, making this result
    // suitable for both firing and displaying the evaluation stages.
    [[nodiscard]] std::vector<RuneWeaponNode> evaluate_weapons() const;

private:
    // Builds one weapon and writes the first slot not claimed by its subtree.
    [[nodiscard]] RuneWeaponNode build_weapon_node(
        int weapon_slot_index,
        int scope_end_index,
        int &subtree_end_index) const;

private:
    std::vector<std::shared_ptr<const Rune>> _runes;
};