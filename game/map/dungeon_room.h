#pragma once

#include "../../engine/core/game_object.h"
#include "../../engine/physics/tile/tile_collision_world.h"

#include "map_generator.h"
#include "tile_map.h"

#include <memory>

struct SDL_Texture;

class DungeonRoom final : public elysia::core::GameObject,
                          public elysia::physics::ITileCollisionWorld
{
public:
    explicit DungeonRoom(
        MapConfig config = {},
        elysia::core::Vector2 grid_size = {50.0f, 50.0f});

    void generate();

    [[nodiscard]] elysia::core::Vector2 tile_render_size() const noexcept;
    [[nodiscard]] const TileMap &tile_map() const noexcept;

    [[nodiscard]] elysia::core::Vector2 world_origin() const noexcept override;
    [[nodiscard]] elysia::core::Vector2 tile_size() const noexcept override;
    [[nodiscard]] int columns() const noexcept override;
    [[nodiscard]] int rows() const noexcept override;
    [[nodiscard]] elysia::physics::TileOutOfBoundsPolicy
    out_of_bounds_policy() const noexcept override;
    [[nodiscard]] elysia::physics::TileCollisionCell cell_at(
        elysia::physics::TileCoordinate coordinate) const noexcept override;

    void submit_render_commands(std::vector<elysia::core::RenderCommand> &commands) const override;

private:
    static constexpr float k_tile_render_size = 32.0f;

    elysia::core::Vector2 _grid_size{25.0f, 25.0f};
    TileMap _tile_map;
    MapConfig _config;
    std::unique_ptr<MapGenerator> _generator;
    SDL_Texture *_tile_sheet_texture = nullptr;
};
