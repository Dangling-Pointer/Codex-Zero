#include "player_controller.h"
#include "game/input/game_input_map.h"

namespace game
{
PlayerController::PlayerController(elysia::input::LocalPlayerId player)
    : elysia::gameplay::LocalPlayerController(player, game::input::make_player_input_map())
{
}
}
