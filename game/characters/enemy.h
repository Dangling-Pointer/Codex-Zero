#pragma once

#include "character.h"

// Basic enemy without autonomous movement or attacks.
class Enemy : public Character
{
public:
    explicit Enemy(elysia::core::Vector2 start_position);
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const override;
};
