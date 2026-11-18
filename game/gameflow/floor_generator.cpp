#include "floor_generator.h"

Floor FloorGenerator::generate(std::uint64_t seed)
{
	_random.set_seed(seed);


	Floor floor;
	elysia::core::Vector2 pos;

	pos = get_start_pos();

	Room start;
	start.accessible_directions = RoomDirection::All;


	return {};
}

elysia::core::Vector2 FloorGenerator::get_start_pos()
{
	elysia::core::Vector2 out;

	out.x = _random.int_inclusive(2, 3);
	out.y = _random.int_inclusive(2, 3);

	return out;
}

elysia::core::Vector2 FloorGenerator::get_end_pos()
{
	elysia::core::Vector2 out;

	int n = _random.int_inclusive(0, 19);

	if (n < 5)
	{
		out.x = n;
		out.y = 0;// up
	}
	else if (n < 10)
	{
		out.x = 5;
		out.y = n - 5;// right
	}
	else if (n < 15)
	{
		out.x = 15 - n;
		out.y = 5;// down
	}
	else
	{
		out.x = 0;
		out.y = 20 - n;// left
	}

	return out;
}
