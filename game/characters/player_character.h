#pragma once

#include "character.h"
#include "engine/gameplay/input/contracts/gameplay_input_frame_receiver.h"

class PlayerCharacter final : public Character,
                              public elysia::gameplay::GameplayInputFrameReceiver
{
public:
    static constexpr float kMoveSpeed = 200.0f;

    explicit PlayerCharacter(elysia::core::Vector2 start_position);
    void on_gameplay_input_frame(const elysia::gameplay::GameplayInputFrame& input) override;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const override;

private:
    float _move_speed = 200.0f;

};
