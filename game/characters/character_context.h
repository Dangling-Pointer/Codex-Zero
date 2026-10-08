#pragma once

#include <string>

using CharacterId = std::string;

// Owned, copyable state carried between rooms. Create new-run state through
// CharacterManager; the default context does not identify a valid character.
struct CharacterContext
{
    CharacterId character_id;
    float health = 0.0f;
    float max_health = 0.0f;
    float move_speed = 0.0f;
};
