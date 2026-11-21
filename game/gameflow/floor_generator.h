#pragma once

#include "dungeon_graph_types.h"
#include "room_factory.h"
#include "engine/tools/random_generator.h"

class FloorGenerator
{
public:
    [[nodiscard]] Floor generate(std::uint64_t seed, RoomID first_room_id, const FloorGenerationConfig& config);

private:
    [[nodiscard]] FloorPosition get_start_pos();
    [[nodiscard]] FloorPosition get_end_pos(FloorPosition start, int room_number);
    void fill_rooms(Floor& floor, int room_number);
    void generate_connections(Floor& floor, int room_number);

    RoomFactory _room_factory;
    elysia::tools::RandomGenerator _random;
};
