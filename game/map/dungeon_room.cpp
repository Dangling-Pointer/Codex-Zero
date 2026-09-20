#include "dungeon_room.h"

#include "../../engine/core/render/render_command.h"
#include "../../engine/resources/resource_service.h"
#include "../../engine/tools/logger.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    constexpr int kTileSpriteSize = 32;
    constexpr float kRoomOriginX = 0.0f;
    constexpr float kRoomOriginY = 0.0f;

    int sprite_index_for_tile(TileType type)
    {
        switch (type)
        {
        case TileType::Void:
            return 13;
        case TileType::Floor:
            return 12;
        case TileType::TopWall:
            return 0;
        case TileType::BottomWall:
            return 1;
        case TileType::LeftWall:
            return 2;
        case TileType::RightWall:
            return 3;
        case TileType::TopLeftCorner:
            return 5;
        case TileType::TopRightCorner:
            return 4;
        case TileType::BottomLeftCorner:
            return 6;
        case TileType::BottomRightCorner:
            return 7;
        case TileType::InnerTopLeft:
            return 10;
        case TileType::InnerTopRight:
            return 11;
        case TileType::InnerBottomLeft:
            return 8;
        case TileType::InnerBottomRight:
            return 9;
        default:
            return 13;
        }
    }
}

DungeonRoom::DungeonRoom(MapConfig config, elysia::core::Vector2 grid_size)
    : elysia::core::GameObject(elysia::core::DepthLayer::Terrain),
      _grid_size{static_cast<float>(std::max(1, static_cast<int>(std::floor(grid_size.x)))),
                 static_cast<float>(std::max(1, static_cast<int>(std::floor(grid_size.y))))},
      _config(config),
      _generator(std::make_unique<MapGenerator>(_grid_size, _config, _tile_map))
{
    set_position({kRoomOriginX, kRoomOriginY});
    set_size({_grid_size.x * k_tile_render_size, _grid_size.y * k_tile_render_size});
    if (elysia::resources::ResourceService::instance())
    {
        _tile_sheet_texture = ELYSIA_RESOURCES->find_texture("room_tiles");
    }

    if (!_tile_sheet_texture)
    {
        ELYSIA_LOG_WARN("gameplay", "DungeonRoom texture not found in resource manager.");
    }

    generate();
}

void DungeonRoom::generate()
{
    _tile_map.resize(static_cast<int>(_grid_size.x), static_cast<int>(_grid_size.y));

    if (!_generator)
    {
        _generator = std::make_unique<MapGenerator>(_grid_size, _config, _tile_map);
    }

    _generator->generateRoom();
}

elysia::core::Vector2 DungeonRoom::tile_render_size() const noexcept
{
    return elysia::core::Vector2(k_tile_render_size, k_tile_render_size);
}

const TileMap &DungeonRoom::tile_map() const noexcept
{
    return _tile_map;
}

elysia::core::Vector2 DungeonRoom::world_origin() const noexcept
{
    return position();
}

elysia::core::Vector2 DungeonRoom::tile_size() const noexcept
{
    return tile_render_size();
}

int DungeonRoom::columns() const noexcept
{
    return _tile_map.width();
}

int DungeonRoom::rows() const noexcept
{
    return _tile_map.height();
}

elysia::physics::TileOutOfBoundsPolicy DungeonRoom::out_of_bounds_policy() const noexcept
{
    return elysia::physics::TileOutOfBoundsPolicy::Block;
}

elysia::physics::TileCollisionCell DungeonRoom::cell_at(
    elysia::physics::TileCoordinate coordinate) const noexcept
{
    const Tile &tile = _tile_map.get(coordinate.x, coordinate.y);

    elysia::physics::TileCollisionCell cell;
    cell.type = tile.collidable
                    ? elysia::physics::TileCollisionType::Block
                    : elysia::physics::TileCollisionType::Empty;

    cell.material.friction = 0.8f;
    cell.material.restitution = 0.0f;

    return cell;
}

void DungeonRoom::submit_render_commands(std::vector<elysia::core::RenderCommand> &commands) const
{
    if (!_tile_sheet_texture)
        return;

    const auto &tiles = _tile_map.data();

    for (int y = 0; y < _tile_map.height(); ++y)
    {
        for (int x = 0; x < _tile_map.width(); ++x)
        {
            const Tile &tile = tiles[y][x];

            elysia::core::RenderCommand command;
            command.texture = _tile_sheet_texture;
            command.command_rect = elysia::core::Rect{
                position().x + static_cast<float>(x) * k_tile_render_size,
                position().y + static_cast<float>(y) * k_tile_render_size,
                k_tile_render_size,
                k_tile_render_size};
            command.use_src_rect = true;
            command.src_rect = elysia::core::Rect{
                static_cast<float>(sprite_index_for_tile(tile.type) * kTileSpriteSize),
                0.0f,
                static_cast<float>(kTileSpriteSize),
                static_cast<float>(kTileSpriteSize)};
            commands.push_back(std::move(command));
        }
    }
}
