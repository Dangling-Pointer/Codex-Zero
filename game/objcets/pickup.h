#pragma once

#include "engine/core/game_object.h"

class Pickup :public elysia::core::GameObject
{
public:
	enum class PickupMode
	{
		Automatic,
		Interact,
	};
public:
	Pickup();
	~Pickup();

private:

};