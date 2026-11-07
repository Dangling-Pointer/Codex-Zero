#pragma once

#include "game/characters/character_context.h"
#include "game/map/map_config.h"

struct RoomBuildConfig
{
    MapConfig map_config{};
    elysia::core::Vector2 grid_size{50.0f, 50.0f};
};

struct RoomScenePayload
{
    CharacterContext character;
    RoomBuildConfig room;
};
