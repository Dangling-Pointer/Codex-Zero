#pragma once

#include "game/map/tile/tile_data.h"
#include "game/map/tile/tile_map.h"
#include "game/map/corridor/room_corridor_layout.h"

#include "engine/core/geometry/rect.h"
#include "engine/tools/random_generator.h"

enum class Side
{
    Top,
    Bottom,
    Left,
    Right,
    None
};

class MapGenerator
{
public:
    MapGenerator(const elysia::core::Vector2 gridDimensions, MapConfig config, TileMap &grid);

    void generateRoom();
    void clearGrid();
    [[nodiscard]] const std::vector<game::RoomCorridorLayout>& corridors() const noexcept { return _corridors; }

private:
    int random_value(int min_value, int max_value);
    void initGrid();

    elysia::core::Rect buildBaseRoom();
    elysia::core::Rect buildSubRoom(elysia::core::Rect &base, Side side = Side::None);
    void validateCorridors() const;
    void buildCorridors(const elysia::core::Rect& base);

    void addRecToGrid(elysia::core::Rect &rec);
    void classifyRecs();

    bool isSolid(const int x, const int y);

    void resetTile(Tile &tile);

private:
    elysia::core::Vector2 _gridDimensions;
    TileMap &grid;

    MapConfig config;
    elysia::tools::RandomGenerator _random;
    std::vector<game::RoomCorridorLayout> _corridors;
};
