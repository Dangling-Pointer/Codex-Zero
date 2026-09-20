#pragma once

#include "tile_data.h"
#include "tile_map.h"

int random_value(int min_value, int max_value);

#include "../../engine/core/geometry/rect.h"

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

private:
    void initGrid();

    elysia::core::Rect buildBaseRoom();
    elysia::core::Rect buildSubRoom(elysia::core::Rect &base, Side side = Side::None);

    void addRecToGrid(elysia::core::Rect &rec);
    void classifyRecs();

    bool isSolid(const int x, const int y);

    void resetTile(Tile &tile);

private:
    elysia::core::Vector2 _gridDimensions;
    TileMap &grid;

    MapConfig config;
};
