#pragma once

#include "../../engine/scene/scene.h"

#include "../../engine/ui/window/ui_window.h"

#include <cstddef>
#include <vector>

namespace elysia::ui
{
	class UiButton;
	class UiConfirmationDialog;
}

namespace game::scene
{

    struct MainMenuEnterPayload
    {
        bool replay_theme_music = false;
    };

class MainMenuScene final : public elysia::scene::Scene
{
public:
    MainMenuScene() = default;
    ~MainMenuScene() override = default;

    void on_update(double delta) override;
    void on_render(SDL_Renderer* renderer) override;
    void on_input(const elysia::input::RawInputFrame& input, const std::vector<elysia::input::RawInputEvent>& events) override;

    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void reset() override;

private:

    void build_menu_buttons();
    void reset_exit_overlay();
    void restore_menu_state();

private:
    bool _has_entered = false;
    elysia::ui::UiWindow* _main_menu_window = nullptr;
    elysia::ui::UiConfirmationDialog* _exit_confirmation = nullptr;
};

}