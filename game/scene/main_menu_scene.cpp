#include "main_menu_scene.h"

#include "run/room_scene_payload.h"
#include "scene_keys.h"
#include "game/characters/character_manager.h"
#include "game/gameflow/game_flow_manager.h"

#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/settings_scene_payload.h"
#include "engine/resources/resource_service.h"
#include "engine/tools/logger.h"
#include "engine/tools/random_generator.h"
#include "engine/ui/composites/ui_confirmation_dialog.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/widgets/image/ui_image.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/window/ui_window.h"

#include <limits>
#include <memory>
#include <utility>

namespace
{
using elysia::core::Rect;
using elysia::scene::SceneRoute;
using elysia::ui::UiButton;
using elysia::ui::UiLayoutAnchor;
using elysia::ui::UiListContainer;
using elysia::ui::UiWindow;
using elysia::ui::ui_raw_text;
using elysia::ui::ui_text_key;

std::unique_ptr<UiButton> make_menu_button(elysia::ui::UiTextContent text,
                                          UiButton::ClickCallback on_click)
{
    static const elysia::ui::UiButtonSounds sounds{
        .press = "system.button_click_down",
        .click = "system.button_click_up",
    };

    auto button = std::make_unique<UiButton>(Rect{0, 0, 250, 75});
    button->set_text_content(std::move(text));
    button->set_sounds(sounds);
    button->set_on_click(std::move(on_click));
    return button;
}
}

void MainMenuScene::on_enter(const elysia::scene::ScenePayload&)
{
    if (_has_entered)
        return;

    if (!_main_menu_window || _main_menu_window->is_destroyed())
        build_menu_buttons();

    restore_menu_state();
    _has_entered = true;
}

void MainMenuScene::on_exit()
{
    reset_exit_overlay();
}

void MainMenuScene::on_reset()
{
    _has_entered = false;
    reset_exit_overlay();
}

void MainMenuScene::build_menu_buttons()
{
    if (_main_menu_window && !_main_menu_window->is_destroyed())
        return;

    const auto width = static_cast<float>(runtime_context().logical_width());
    const auto height = static_cast<float>(runtime_context().logical_height());
    _main_menu_window = create_and_add_object<UiWindow>(Rect{0, 0, width, height}, 10);
    _exit_confirmation = nullptr;

    if (!_main_menu_window)
        return;

    auto& window = *_main_menu_window;
    _exit_confirmation = window.create_child<elysia::ui::UiConfirmationDialog>(Rect{0, 0, 420, 240}, 10);
    if (_exit_confirmation)
    {
        _exit_confirmation->set_config({
            .title = ui_text_key("menu_scene.exit_confirm.title"),
            .message = ui_text_key("menu_scene.exit_confirm.message"),
            .confirm = ui_text_key("menu_scene.exit"),
            .cancel = ui_text_key("menu_scene.exit_confirm.cancel"),
            .close = ui_text_key("menu_scene.exit_confirm.close"),
            .confirm_visual_role = elysia::ui::UiButtonVisualRole::Danger,
        });
        _exit_confirmation->set_on_confirm([this] { request_quit(); });
        (void)_exit_confirmation->register_with_window(window);
    }

    const auto show_exit_confirmation = [this] {
        if (_exit_confirmation)
            _exit_confirmation->open();
    };

    auto menu_buttons = std::make_unique<UiListContainer>(Rect{0, 0, 300, 500});
    menu_buttons->add_back(make_menu_button(ui_text_key("menu_scene.start"), [this] { start_run(); }));
    menu_buttons->add_back(make_menu_button(ui_text_key("menu_scene.settings"), [this] {
        const SceneRoute return_route{
            .target = MainMenu,
            .payload = MainMenuEnterPayload{.replay_theme_music = false},
            .reload_mode = elysia::scene::SceneReloadMode::Reuse,
        };
        request_scene_switch(elysia::builtin::SceneKeys::Settings,
                             elysia::builtin::SettingsScenePayload{.return_route = return_route});
    }));
    menu_buttons->add_back(make_menu_button(ui_text_key("menu_scene.about"), [] {
        ELYSIA_LOG_DEBUG("MainMenuScene", "menu_scene.about button clicked");
    }));
    menu_buttons->add_back(make_menu_button(ui_text_key("menu_scene.exit"), show_exit_confirmation));

    auto test_button = make_menu_button(ui_text_key("menu_scene.test"), [this] { request_scene_switch(Game); });
    window.add_child(std::move(test_button), {._anchor = UiLayoutAnchor::TopRight});

    auto character_button = make_menu_button(ui_raw_text("character selection"), [] {
        ELYSIA_LOG_DEBUG("MainMenuScene", "Character Selection test button");
    });
    window.add_child(std::move(character_button), {._anchor = UiLayoutAnchor::TopLeft});

    auto logo = std::make_unique<elysia::ui::UiImage>(
        ELYSIA_RESOURCES->find_texture("ui.codex_zero"), Rect{0, 0, 512, 168});
    auto menu_content = std::make_unique<UiListContainer>(Rect{0, 0, 600, 600});
    menu_content->add_front(std::move(logo));
    menu_content->add_back(std::move(menu_buttons));

    auto* content = window.add_child(std::move(menu_content), {._anchor = UiLayoutAnchor::Center});
    if (auto* list = dynamic_cast<UiListContainer*>(content))
        window.register_focus_scope(*list);

    window.set_style_overrides({.draw_background = true, .draw_border = false});
    window.set_on_cancel(show_exit_confirmation);
}

void MainMenuScene::start_run()
{
    const auto character = CharacterManager::instance()->create_context("default_player");
    if (!character)
    {
        ELYSIA_LOG_ERROR("MainMenuScene", "Cannot create default player context");
        return;
    }

    using Seed = elysia::tools::RandomGenerator::Seed;
    elysia::tools::RandomGenerator random;
    const auto seed = random.int_inclusive<Seed>(0, std::numeric_limits<Seed>::max());

    auto& flow = *GameFlowManager::instance();
    flow.restart(seed);
    request_scene_switch(make_run_room_route(flow.get_current_room_id(), *character));
}

void MainMenuScene::reset_exit_overlay()
{
    if (!_main_menu_window || _main_menu_window->is_destroyed())
        return;

    if (_exit_confirmation && !_exit_confirmation->is_destroyed())
        _exit_confirmation->close();
}

void MainMenuScene::restore_menu_state()
{
    if (!_main_menu_window || _main_menu_window->is_destroyed())
        return;

    _main_menu_window->set_active(true);
    _main_menu_window->set_visible(true);
    reset_exit_overlay();
    _main_menu_window->focus_first_available_scope();
}
