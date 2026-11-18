#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

using RoomID = std::uint32_t;
using DungeonDepth = std::uint8_t;

enum class RoomDirection : std::uint8_t
{
    None = 0,
    North = 1 << 0,
    East = 1 << 1,
    South = 1 << 2,
    West = 1 << 3,
    All = North | East | South | West,
};

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
    RoomDirection accessible_directions = RoomDirection::None;
    std::optional<std::string> content_id{};

    bool discovered = false;
    bool visited = false;
    bool cleared = false;
    bool event_triggered = false;
};

struct FloorPosition
{
    int x;
    int y;

    bool operator==(const FloorPosition&) const noexcept = default;
};

struct Floor
{
    static constexpr int width = 6;
    static constexpr int height = 6;

    std::array<std::array<std::optional<Room>, width>, height> rooms{};

    RoomID start_room_id = 0;
    RoomID end_room_id = 0;
    RoomID current_room_id = 0;
    std::uint64_t seed = 0;

    [[nodiscard]] Room* room_at(FloorPosition position) noexcept;
    [[nodiscard]] const Room* room_at(FloorPosition position) const noexcept;
    [[nodiscard]] std::optional<FloorPosition> find_position(RoomID room_id) const noexcept;
    [[nodiscard]] std::array<std::optional<FloorPosition>, 4> neighbors(FloorPosition position) const noexcept;
    [[nodiscard]] bool connect(FloorPosition first, FloorPosition second) noexcept;
};
