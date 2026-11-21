#pragma once

#include "engine/scene/scene.h"

namespace elysia::ui
{
class UiWindow;
class UiConfirmationDialog;
}

struct MainMenuEnterPayload
{
    bool replay_theme_music = false;
};

class MainMenuScene final : public elysia::scene::Scene
{
public:
    MainMenuScene() = default;
    ~MainMenuScene() override = default;

    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void on_reset() override;

private:
    void build_menu_buttons();
    void start_run();
    void start_corridor_test();
    void reset_exit_overlay();
    void restore_menu_state();

    bool _has_entered = false;
    elysia::ui::UiWindow* _main_menu_window = nullptr;
    elysia::ui::UiConfirmationDialog* _exit_confirmation = nullptr;
};
