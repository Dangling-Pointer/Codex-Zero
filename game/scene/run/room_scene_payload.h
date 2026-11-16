#pragma once

#include "game/characters/character_context.h"
#include "game/gameflow/dungeon_graph_types.h"
#include "game/map/map_config.h"
#include "engine/scene/routing/scene_route.h"

#include <optional>

struct RoomBuildConfig
{
    MapConfig map_config{};
    elysia::core::Vector2 grid_size{50.0f, 50.0f};
};

struct RoomScenePayload
{
    CharacterContext character;
    RoomBuildConfig room;
    std::optional<RoomID> room_id;
    std::optional<RoomID> from_room;
};

struct FloorMapPayload
{
    RoomID from_room;
    CharacterContext character;
};

[[nodiscard]] elysia::scene::SceneRoute make_run_room_route(
    RoomID room_id, const CharacterContext& character, std::optional<RoomID> from_room = std::nullopt);
