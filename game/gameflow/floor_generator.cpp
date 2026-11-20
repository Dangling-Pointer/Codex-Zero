#include "floor_generator.h"

#include <limits>
#include <stdexcept>

Floor FloorGenerator::generate(std::uint64_t seed, RoomID first_room_id,const FloorGenerationConfig& config)
{
    if (first_room_id == std::numeric_limits<RoomID>::max())
        throw std::overflow_error("Not enough room IDs for the floor's start and exit");

    _random.set_seed(seed);

    Floor floor{
        .start_room_id = first_room_id,
        .end_room_id = first_room_id + 1,
        .current_room_id = first_room_id,
        .seed = seed,
    };

    const FloorPosition start = get_start_pos();
    floor.rooms[start.y][start.x] = _room_factory.create_start(floor.start_room_id);

    const FloorPosition end = get_end_pos();
    floor.rooms[end.y][end.x] = _room_factory.create_end(floor.end_room_id);


    return floor;
}

FloorPosition FloorGenerator::get_start_pos()
{
    return {
        _random.int_inclusive(2, 3),
        _random.int_inclusive(2, 3),
    };
}

FloorPosition FloorGenerator::get_end_pos()
{
    // Walk around the perimeter, including each corner exactly once.
    static_assert(Floor::width == 6 && Floor::height == 6);

    const int index = _random.int_inclusive(0, 19);
    if (index < 5)
        return {index, 0};
    if (index < 10)
        return {5, index - 5};
    if (index < 15)
        return {15 - index, 5};

    return {0, 20 - index};
}
