#pragma once

#include "game/characters/character_context.h"
#include "game/characters/player_motion_state.h"
#include "game/gameflow/dungeon_graph_types.h"
#include "game/map/map_config.h"
#include "game/map/corridor_layout.h"
#include "engine/scene/routing/scene_route.h"

#include <optional>

struct RoomBuildConfig
{
    MapConfig map_config{};
    elysia::core::Vector2 grid_size{50.0f, 50.0f};
};

struct RoomBoundaryEntry
{
    RoomDirection direction = RoomDirection::None; // Destination's entrance.
    elysia::core::Vector2 seam_offset;
    PlayerMotionState motion;
    elysia::core::Vector2 camera_offset; // Relative to the rendered player center.
    float camera_zoom = 2.0f;
};

struct RoomScenePayload
{
    CharacterContext character;
    RoomBuildConfig room;
    std::optional<RoomID> room_id;
    std::optional<RoomID> from_room;
    std::optional<RoomBoundaryEntry> boundary_entry;
    std::optional<CorridorTraversal> corridor;
};

struct FloorMapPayload
{
    RoomID from_room;
    CharacterContext character;
};

// Navigation rules only; character and map configuration are validated separately.
void validate_room_entry(const RoomScenePayload& payload);

[[nodiscard]] elysia::scene::SceneRoute make_run_room_route(
    RoomID room_id, const CharacterContext& character, std::optional<RoomID> from_room = std::nullopt);

[[nodiscard]] bool supports_room_corridors(RoomType type) noexcept;
[[nodiscard]] elysia::scene::SceneRoute make_boundary_room_route(
    RoomID from, RoomDirection exit, const CharacterContext& character, RoomBoundaryEntry entry);

[[nodiscard]] elysia::scene::SceneRoute make_corridor_room_route(
    const CorridorTraversal& traversal, RoomDirection exit,
    const CharacterContext& character, RoomBoundaryEntry entry);
