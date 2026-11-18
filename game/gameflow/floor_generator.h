#pragma once
#include "dungeon_graph_types.h"

#include "engine/core/geometry/vector2.h"
#include "engine/tools/random_generator.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>


class FloorGenerator
{
public:

	[[nodiscard]] Floor generate(std::uint64_t seed);

private:
	[[nodiscard]] elysia::core::Vector2 get_start_pos();
	[[nodiscard]] elysia::core::Vector2 get_end_pos();

private:
	std::array<std::string, 10> _special_event_pool;
	elysia::tools::RandomGenerator _random;
};