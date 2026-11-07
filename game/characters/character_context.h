#pragma once

#include <string>

using CharacterId = std::string;

struct CharacterContext
{
    CharacterId character_id;
    float health = 0.0f;
    float max_health = 0.0f;
    float move_speed = 0.0f;
};
