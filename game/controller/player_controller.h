#pragma once

#include "engine/gameplay/control/controller.h"

namespace game
{
class PlayerController : public elysia::gameplay::LocalPlayerController
{
public:
    explicit PlayerController(
        elysia::input::LocalPlayerId player = elysia::input::PrimaryLocalPlayer);
};
}
