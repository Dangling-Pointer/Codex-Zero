#pragma once

#include "engine/scene/scene.h"
#include "engine/tools/timer.h"

#include <optional>

class TransitionScene final : public elysia::scene::Scene
{
protected:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void on_reset() override;

private:

private:
    elysia::tools::Timer _transition_timer;
    std::optional<elysia::scene::SceneRoute> _next_route;
};