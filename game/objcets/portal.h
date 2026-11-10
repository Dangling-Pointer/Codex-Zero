#pragma once

#include "engine/core/game_object.h"

#include <functional>
#include <utility>

class Portal : public elysia::core::GameObject
{
public:
    explicit Portal(elysia::core::Rect bounds);
    void check_reached(const elysia::core::Rect& player_bounds);
    void set_on_reached(std::function<void()> callback) { _on_reached = std::move(callback); }
    [[nodiscard]] bool is_reached() const noexcept { return _reached; }
    void reset() noexcept override;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override;

private:
    void reach();

private:
    std::function<void()> _on_reached;
    bool _reached = false;
};
