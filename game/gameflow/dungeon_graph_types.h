#pragma once

#include <cstdint>
#include <vector>

using RoomID = std::uint32_t;
using DungeonDepth = std::uint8_t;

enum class RoomType : std::uint8_t
{
    Start,
    End,
    Combat,
    Elite,
    MiniBoss,
    Boss,
    Shop,
    Event,
    Rest,
    NONE
};

struct Room
{
    RoomID id = 0;
    RoomType type = RoomType::NONE;
    int x = 0;
    int y = 0;
    std::vector<RoomID> neighbors{};
    bool discovered = false;
    bool visited = false;
    bool cleared = false;
};
