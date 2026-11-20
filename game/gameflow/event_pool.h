#pragma once
#include "engine/tools/random_generator.h"

#include <string>

class EventPool
{
public:
	void set_seed(std::uint64_t seed);

	std::string get_event_random_event();

private:
	elysia::tools::RandomGenerator _random;
	std::array<std::string, 10> _special_event_pool;

};
