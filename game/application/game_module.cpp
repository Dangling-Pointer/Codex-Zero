#include "game_module.h"

#include "../scene/scene_keys.h"
#include "../scene/main_menu_scene.h"
#include "../scene/room_scene.h"
#include "../scene/game_scene.h"

#include "../../engine/builtin/builtin_scene_keys.h"
#include "../../engine/builtin/scenes/startup_loading_scene.h"
#include "../../engine/scene/scene_manager.h"

#if ELYSIA_ENABLE_IMGUI
#include "../../engine/tools/imgui/imgui_development_overlay.h"
#endif

elysia::application::ApplicationDescriptor GameModule::descriptor() const
{
    using elysia::builtin::StartupLoadingScenePayload;
    using elysia::scene::SceneReloadMode;
    using elysia::scene::SceneRoute;

    elysia::application::ApplicationDescriptor descriptor;
    descriptor.logical_width = 1280;
    descriptor.logical_height = 720;
    descriptor.presentation.render.texture_filter = elysia::application::ApplicationTextureFilter::Nearest;
    descriptor.presentation.ui.default_theme = elysia::ui::UiBuiltinTheme::BlueGlassMoon;
    descriptor.presentation.startup.engine_logo = elysia::application::ApplicationEngineLogoVariant::White;
    descriptor.presentation.fonts.ui.source = elysia::typography::FontSource::Project;
    descriptor.presentation.fonts.floating_number.source = elysia::typography::FontSource::Project;
    descriptor.initial_route = SceneRoute{
        .target = elysia::builtin::SceneKeys::StartupLoading,
        .payload = StartupLoadingScenePayload{
            .success_route = SceneRoute{
                .target = MainMenu,
                .payload = MainMenuEnterPayload{},
                .reload_mode = SceneReloadMode::Reuse},
            .failure_route = std::nullopt,
            .project_logo = elysia::builtin::StartupLogoSlot{.texture_key = "dangling_ptr"},
            .wait_for_confirmation = true},
        .reload_mode = SceneReloadMode::Reuse};

    return descriptor;
}

void GameModule::register_scenes(elysia::scene::SceneManager &scene_manager) const
{
    scene_manager.register_game_scene<MainMenuScene>(MainMenu);
    scene_manager.register_game_scene<RoomScene>(Room);
    scene_manager.register_game_scene<GameScene>(Game);
}

std::unique_ptr<elysia::tools::IDevelopmentOverlay>
GameModule::create_development_overlay() const
{
#if ELYSIA_ENABLE_IMGUI
    return std::make_unique<elysia::tools::ImGuiDevelopmentOverlay>();
#else
    return {};
#endif
}
