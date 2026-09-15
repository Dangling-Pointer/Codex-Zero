#include "application.h"
#include "../builtin/resources/builtin_resources.h"

#include "composition/application_scene_composition.h"
#include "lifecycle/application_event_boundary.h"
#include "lifecycle/application_exit_policy.h"
#include "lifecycle/application_termination_logging.h"
#include "presentation/application_sdl_presentation.h"
#include "presentation/application_window_settings.h"

#include "../builtin/resources/builtin_asset_catalog.h"
#include "../audio/audio_service.h"
#include "../bootstrap/bootstrapper.h"
#include "../core/time.h"
#include "../effects/runtime/effect_manager.h"
#include "../loading/content_runtime_cleanup.h"
#include "../localization/localization_manager.h"
#include "../localization/localization_service.h"
#include "../io/path/path_manager.h"
#include "../resources/resource_service.h"
#include "../save/save_service.h"
#include "../tools/logger.h"
#include "../ui/style/ui_theme_defaults.h"

#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
#include <utility>

#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

namespace elysia::application
{
namespace
{
[[nodiscard]] std::uint32_t calculate_frame_delay_milliseconds(
    double target_fps,
    double elapsed_seconds) noexcept
{
    if (!std::isfinite(target_fps) || target_fps <= 0.0
        || !std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0)
    {
        return 0;
    }

    const double remaining_seconds = 1.0 / target_fps - elapsed_seconds;
    if (remaining_seconds <= 0.0)
        return 0;

    const double delay_milliseconds =
        std::ceil(remaining_seconds * 1000.0);
    constexpr auto maximum_delay =
        std::numeric_limits<std::uint32_t>::max();
    if (delay_milliseconds >= static_cast<double>(maximum_delay))
        return maximum_delay;

    return static_cast<std::uint32_t>(delay_milliseconds);
}
}

Application::~Application()
{
    shutdown();
}

bool Application::check_startup_step(
    bool flag,
    std::string_view category,
    const char* err_msg,
    std::source_location location)
{
    if (flag)
        return true;

    const std::string error_message =
        err_msg ? err_msg : "Application runtime initialization failed.";
    return startup_fail(category,error_message,location);
}

bool Application::startup_fail(
    std::string_view category,
    const std::string& err_msg,
    std::source_location location)
{
    auto* logger = elysia::tools::Logger::instance();
    logger->error(category,err_msg,location);
    logger->terminating(
        "application",
        "Application terminating during startup after a fatal failure",
        location);
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR,
        "Game Start Error",
        err_msg.c_str(),
        _window);
    shutdown();
    return false;
}

bool Application::startup_fail(
    const elysia::bootstrap::BootstrapFailure& failure)
{
    std::filesystem::path project_root;
    if (failure.code != elysia::bootstrap::BootstrapFailure::Code::ProjectRoot)
        if (const auto* paths = elysia::io::PathManager::instance();
            paths && paths->is_initialized())
            project_root = paths->root();
    const std::string formatted = elysia::core::format_failure_diagnostic(
        failure.diagnostic,failure.error_code(),"bootstrap",project_root);
    auto* logger = elysia::tools::Logger::instance();
    logger->error("bootstrap",formatted,failure.diagnostic.origin);
    logger->terminating(
        "application",
        "Application terminating during startup after a fatal failure",
        failure.diagnostic.origin);

    std::string dialog = "The game could not start.\nError code: ";
    dialog += failure.error_code();
    if (failure.code == elysia::bootstrap::BootstrapFailure::Code::ProjectRoot)
        dialog += "\nRequired marker: assets/.elysia_root";
#if !defined(NDEBUG)
    dialog += "\n\n" + formatted;
#endif
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR,"Game Start Error",dialog.c_str(),_window);
    shutdown();
    return false;
}

bool Application::startup_fail(
    const elysia::localization::LocalizationFailure& failure)
{
    std::filesystem::path project_root;
    if (const auto* paths = elysia::io::PathManager::instance();
        paths && paths->is_initialized())
        project_root = paths->root();
    const std::string formatted = elysia::core::format_failure_diagnostic(
        failure.diagnostic,failure.error_code(),"localization",project_root);
    auto* logger = elysia::tools::Logger::instance();
    logger->error("localization",formatted,failure.diagnostic.origin);
    logger->terminating(
        "application",
        "Application terminating during startup after a fatal failure",
        failure.diagnostic.origin);
    std::string dialog = "The game could not start.\nError code: ";
    dialog += failure.error_code();
#if !defined(NDEBUG)
    dialog += "\n\n" + formatted;
#endif
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR,"Game Start Error",dialog.c_str(),_window);
    shutdown();
    return false;
}

bool Application::initialize(
    int argc,
    char** argv,
    const IGameModule& game_module)
{
    elysia::tools::Logger::instance()->initialize_console();

    _active = true;
    _normal_exit_requested = false;
    _has_shutdown = false;

    elysia::tools::TerminationManager::instance()->initialize_lifecycle();

    ApplicationDescriptor descriptor;
    try
    {
        descriptor = describe_game_module(game_module);
    }
    catch (const std::exception& error)
    {
        return startup_fail(
            "game_module",
            std::string("Game module descriptor failed: ") + error.what());
    }
    catch (...)
    {
        return startup_fail(
            "game_module",
            "Game module descriptor failed with an unknown exception.");
    }

    if (descriptor.logical_width <= 0 || descriptor.logical_height <= 0)
        return startup_fail("game_module","Game module logical viewport must be positive.");

    const auto resolved_font_settings =
        elysia::typography::resolve_font_settings(
            descriptor.presentation.fonts);
    if (!resolved_font_settings)
        return startup_fail("typography",resolved_font_settings.error());

    if (!elysia::ui::UiThemeDefaults::set_builtin_theme(
            descriptor.presentation.ui.default_theme))
    {
        return startup_fail(
            "ui",
            "Application default UI theme is invalid.");
    }

    const std::filesystem::path executable_path =
        argc > 0 && argv && argv[0]
            ? std::filesystem::path(argv[0])
            : std::filesystem::path{};
    auto parse_result =
        elysia::bootstrap::Bootstrapper::instance()->parse_runtime_settings(
            executable_path);

    if (!parse_result)
        return startup_fail(parse_result.error());

    elysia::bootstrap::BootstrapOutput bootstrap_output =
        std::move(*parse_result);
    _content_registry = std::move(bootstrap_output.content_registry);
    elysia::tools::Logger::instance()->initialize_file();

    if (const auto save_result = ELYSIA_SAVE->initialize(
            elysia::io::PathManager::instance()->saves());
        !save_result)
    {
        return startup_fail("save",save_result.error().message);
    }

    if (bootstrap_output.warning)
        ELYSIA_LOG_WARN(
            "application",
            elysia::core::format_failure_diagnostic(
                bootstrap_output.warning->diagnostic,
                "BOOTSTRAP-USER-CONFIG","user-config",
                elysia::io::PathManager::instance()->root()));

    elysia::bootstrap::RuntimeSettings runtime_settings =
        std::move(bootstrap_output.runtime_settings);
    if (!initialize_runtime(runtime_settings,descriptor))
        return false;

#if ELYSIA_ENABLE_IMGUI
    try
    {
        _development_overlay_host.set_overlay(
            game_module.create_development_overlay());
    }
    catch (const std::exception& error)
    {
        return startup_fail(
            "development_overlay",
            std::string("Development overlay creation failed: ")
                + error.what());
    }
    catch (...)
    {
        return startup_fail(
            "development_overlay",
            "Development overlay creation failed with an unknown exception.");
    }
    if (_development_overlay_host.configured())
    {
        try
        {
            auto overlay_result = _development_overlay_host.initialize(
                *_window, *_renderer);
            if (!overlay_result)
            {
                return startup_fail(
                    "development_overlay", overlay_result.error());
            }
        }
        catch (const std::exception& error)
        {
            return startup_fail(
                "development_overlay",
                std::string("Development overlay initialization failed: ")
                    + error.what());
        }
        catch (...)
        {
            return startup_fail(
                "development_overlay",
                "Development overlay initialization failed with an unknown exception.");
        }
    }
#endif

    const elysia::builtin::BuiltinAssetCatalog builtin_asset_catalog(
        *elysia::io::PathManager::instance());
    if (const auto builtin_asset_result = elysia::builtin::BuiltinResources::instance()->initialize(
            _renderer,
            builtin_asset_catalog,
            resolved_font_settings->engine_point_sizes(),
            runtime_settings.user.audio);
        !builtin_asset_result)
    {
        return startup_fail(
            "builtin",
            "Built-in asset initialization failed: " + builtin_asset_result.error());
    }
    if (auto localization_result =
        elysia::localization::LocalizationManager::instance()->initialize(
        _renderer,
        bootstrap_output.i18n_manifest_path,
        runtime_settings.user.language,
        &_font_resolver);
        !localization_result)
    {
        return startup_fail(localization_result.error());
    }

    if (const auto font_result = _font_resolver.configure(
            *resolved_font_settings,
            *elysia::resources::ResourceService::instance(),
            ELYSIA_LOCALIZATION->supported_languages());
        !font_result)
    {
        return startup_fail("typography",font_result.error().message);
    }
    elysia::effects::EffectManager::instance()->set_runtime_dependencies(
        _renderer,
        &_font_resolver);

    elysia::config::UserConfigService::instance()->register_user_config_change_handler(*this);
    _user_config_handler_registered = true;

    elysia::config::UserConfig& user_config =
        elysia::config::UserConfigService::instance()->user_config();
    if (user_config.language()
        != ELYSIA_LOCALIZATION->current_language())
    {
        const auto language_result = user_config.set_language(
            ELYSIA_LOCALIZATION->current_language());
        if (!language_result)
        {
            ELYSIA_LOG_WARN("application",
                "Localization warning: normalize language in config failed: "
                << language_result.error().message);
        }
        else if (const auto save_result =
            elysia::config::UserConfigService::instance()->save_user_config();
            !save_result)
        {
            ELYSIA_LOG_WARN("application",
                "Localization warning: save normalized language failed: "
                << save_result.error().message);
        }
    }

    _input_system.initialize();
    _input_system.set_renderer(_renderer);

    if (const auto preload_result =
            elysia::bootstrap::Bootstrapper::instance()
                ->preload_startup_resources(_renderer);
        !preload_result)
        return startup_fail(preload_result.error());

    elysia::tools::IDevelopmentPanelRegistry* development_panels = nullptr;
#if ELYSIA_ENABLE_IMGUI
    development_panels = _development_overlay_host.panel_registry();
#endif
    _scene_runtime_context.emplace(
        _renderer,
        _content_registry,
        descriptor.logical_width,
        descriptor.logical_height,
        &_font_resolver,
        development_panels);
    _scene_manager.set_runtime_context(*_scene_runtime_context);

    return enter_initial_scene(game_module,descriptor);
}

bool Application::initialize_runtime(
    const elysia::bootstrap::RuntimeSettings& settings,
    const ApplicationDescriptor& descriptor)
{
    const elysia::config::UserConfigData& user_settings = settings.user;
    const bool sdl_initialized = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD);
    _sdl_initialized = sdl_initialized;
    if (!check_startup_step(sdl_initialized,"platform","SDL3 Error"))
        return false;

    if (const auto presentation_result =
            detail::configure_sdl_render_hints(descriptor.presentation.render);
        !presentation_result)
    {
        return startup_fail("platform",presentation_result.error());
    }

    _mixer_initialized = MIX_Init();
    if (!check_startup_step(_mixer_initialized,"audio","SDL_mixer Error"))
        return false;

    const bool ttf_initialized = TTF_Init();
    _ttf_initialized = ttf_initialized;
    if (!check_startup_step(ttf_initialized,"platform","SDL_ttf Error"))
        return false;

    if (!check_startup_step(
        elysia::audio::AudioService::instance()->initialize(user_settings.audio),
        "audio",
        "AudioService initialization failed"))
    {
        return false;
    }



    _window = SDL_CreateWindow(settings.window_title.c_str(), user_settings.window.windowed_size.width, user_settings.window.windowed_size.height, 0);
    if (!check_startup_step(_window != nullptr,"platform","SDL_CreateWindow Error"))
        return false;

    if (user_settings.window.mode
            == elysia::config::WindowMode::BorderlessFullscreen
        && !SDL_SetWindowFullscreen(_window,SDL_WINDOW_FULLSCREEN))
    {
        ELYSIA_LOG_WARN(
            "application",
            "Failed to enter borderless fullscreen: " << SDL_GetError());
        SDL_ClearError();
        SDL_SetWindowSize(
            _window,
            user_settings.window.windowed_size.width,
            user_settings.window.windowed_size.height);
        SDL_SetWindowPosition(_window,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);
    }

    _renderer = SDL_CreateGPURenderer(nullptr,_window);
    if (!check_startup_step(_renderer != nullptr,"platform","SDL GPU renderer creation failed"))
        return false;
    ELYSIA_LOG("application","SDL3 GPU backend: " << SDL_GetGPUDeviceDriver(SDL_GetGPURendererDevice(_renderer)));
    if (!check_startup_step(SDL_SetRenderVSync(_renderer,user_settings.vsync ? 1 : 0),"platform","SDL VSync configuration failed"))
        return false;
    if (auto filter_result = detail::configure_sdl_texture_filter(_renderer,descriptor.presentation.render); !filter_result)
        return startup_fail("platform",filter_result.error());

    if (const auto presentation_result =
            detail::configure_sdl_renderer_presentation(
            _renderer,
            descriptor.logical_width,
            descriptor.logical_height);
        !presentation_result)
    {
        return startup_fail("platform",presentation_result.error());
    }

    _target_fps = user_settings.target_fps;
    return true;
}

bool Application::enter_initial_scene(
    const IGameModule& game_module,
    const ApplicationDescriptor& descriptor)
{
    _scene_manager.attach(this);

    try
    {
        compose_application_scenes(
            _scene_manager,
            game_module,
            descriptor);
    }
    catch (const std::exception& error)
    {
        return startup_fail(
            "scene",
            std::string("Scene composition failed: ") + error.what());
    }
    catch (...)
    {
        return startup_fail(
            "scene",
            "Scene composition failed with an unknown exception.");
    }

    return true;
}

ApplicationRunResult Application::run()
{
    std::uint64_t last_frame_start = SDL_GetPerformanceCounter();
    const std::uint64_t counter_freq = SDL_GetPerformanceFrequency();
    elysia::core::Time::instance()->reset();

    ApplicationRunResult run_result = ApplicationRunResult::NormalExit;
    auto resolve_exit = [this,&run_result]()
    {
        auto* termination_manager = elysia::tools::TerminationManager::instance();
        const ApplicationExitDecision decision =
            resolve_application_exit(_normal_exit_requested,*termination_manager);
        if (decision == ApplicationExitDecision::Continue)
            return false;

        _active = false;
        run_result = to_application_run_result(decision);
        log_fault_exit_if_needed(decision,termination_manager->termination_info());
        return true;
    };
    auto stop_after_boundary_failure = [this,&run_result,&resolve_exit]()
    {
        if (resolve_exit())
            return;

        _active = false;
        run_result = ApplicationRunResult::FaultExit;
        log_published_termination(std::nullopt);
    };

    while (_active)
    {
        const std::uint64_t frame_start = SDL_GetPerformanceCounter();
        const double delta =
            static_cast<double>(frame_start - last_frame_start) / counter_freq;
        last_frame_start = frame_start;
        elysia::core::Time::instance()->begin_frame(delta);

#if ELYSIA_ENABLE_IMGUI
        _input_system.set_development_input_capture(
            _development_overlay_host.captured_input());
#endif
        _input_system.begin_frame();
        while (SDL_PollEvent(&_event))
        {
            bool development_event_consumed = false;
#if ELYSIA_ENABLE_IMGUI
            development_event_consumed =
                _development_overlay_host.process_event(_event);
#endif
            if (!development_event_consumed)
                _input_system.process_event(_event);
            if (_event.type == SDL_EVENT_QUIT)
                _normal_exit_requested = true;
        }

        _input_system.end_frame();
        if (resolve_exit())
            break;

        if (!run_event_boundary("input",[this]()
        {
            _scene_manager.on_input(_input_system.frame(),_input_system.events());
        }))
        {
            stop_after_boundary_failure();
            break;
        }
        if (resolve_exit())
            break;

        if (!run_event_boundary("update",[this]()
        {
            const double frame_delta = elysia::core::Time::instance()->delta();
#if ELYSIA_ENABLE_IMGUI
            _development_overlay_host.begin_frame(frame_delta);
#endif
            _scene_manager.on_update(frame_delta);
            elysia::audio::AudioService::instance()->update(frame_delta);
        }))
        {
            stop_after_boundary_failure();
            break;
        }
        if (resolve_exit())
            break;

        SDL_SetRenderDrawColor(_renderer,0,0,0,255);
        SDL_RenderClear(_renderer);

        if (!run_event_boundary("render",[this]()
        {
            _scene_manager.on_render(_renderer);
#if ELYSIA_ENABLE_IMGUI
            _development_overlay_host.render(*_renderer);
#endif
        }))
        {
            stop_after_boundary_failure();
            break;
        }
        if (resolve_exit())
            break;

        SDL_RenderPresent(_renderer);
        if (resolve_exit())
            break;

        const std::uint64_t frame_end = SDL_GetPerformanceCounter();
        const double elapsed_seconds =
            static_cast<double>(frame_end - frame_start) / counter_freq;
        const std::uint32_t delay_milliseconds =
            calculate_frame_delay_milliseconds(_target_fps,elapsed_seconds);
        if (delay_milliseconds > 0)
            SDL_Delay(delay_milliseconds);
    }

    shutdown();
    return run_result;
}

void Application::shutdown()
{
    if (_has_shutdown)
        return;

    _has_shutdown = true;
    _active = false;

    _input_system.shutdown();
    _input_system.set_renderer(nullptr);
    _scene_manager.detach(this);
    _scene_manager.shutdown();
#if ELYSIA_ENABLE_IMGUI
    _development_overlay_host.shutdown();
#endif
    _scene_runtime_context.reset();
    ELYSIA_SAVE->shutdown();

    elysia::localization::LocalizationManager::instance()->shutdown();
    elysia::bootstrap::Bootstrapper::instance()->release_preload_textures();
    _font_resolver.deactivate_project_fonts();
    elysia::effects::EffectManager::instance()->set_runtime_dependencies(nullptr,nullptr);
    elysia::audio::AudioService::instance()->shutdown();
    elysia::loading::clear_loaded_content();
    _font_resolver.shutdown();
    elysia::builtin::BuiltinResources::instance()->shutdown();
    if (_user_config_handler_registered)
    {
        elysia::config::UserConfigService::instance()
            ->unregister_user_config_change_handler(*this);
        _user_config_handler_registered = false;
    }
    elysia::config::UserConfigService::instance()->shutdown();

    SDL_DestroyRenderer(_renderer);
    _renderer = nullptr;
    SDL_DestroyWindow(_window);
    _window = nullptr;

    if (_ttf_initialized)
    {
        TTF_Quit();
        _ttf_initialized = false;
    }
    if (_mixer_initialized)
    {
        elysia::audio::detail::mixer_backend().shutdown();
        MIX_Quit();
        _mixer_initialized = false;
    }
    if (_sdl_initialized)
    {
        SDL_Quit();
        _sdl_initialized = false;
    }

    ELYSIA_LOG("application","Application shutdown complete");
    elysia::tools::Logger::instance()->shutdown();
}

void Application::on_scene_manager_quit_requested()
{
    _normal_exit_requested = true;
}

namespace
{
std::unexpected<elysia::config::UserConfigFailure> runtime_apply_failure(
    const char* setting,
    const std::string& message)
{
    return std::unexpected(elysia::config::UserConfigFailure{
        elysia::config::UserConfigError::RuntimeApplyFailed,
        setting,
        message
    });
}
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_master_volume(int value)
{
    elysia::audio::AudioService::instance()->set_master_volume(value);
    elysia::builtin::BuiltinResources::instance()->set_master_volume(value);
    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_music_volume(int value)
{
    elysia::audio::AudioService::instance()->set_music_volume(value);
    elysia::builtin::BuiltinResources::instance()->set_music_volume(value);
    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_sound_volume(int value)
{
    elysia::audio::AudioService::instance()->set_sound_volume(value);
    elysia::builtin::BuiltinResources::instance()->set_sound_volume(value);
    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_language(std::string_view language)
{
    if (auto result = ELYSIA_LOCALIZATION->set_language(std::string(language));
        !result)
    {
        return runtime_apply_failure("language",result.error().diagnostic.message);
    }

    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_target_fps(double value)
{
    if (!std::isfinite(value) || value <= 0.0)
    {
        return runtime_apply_failure(
            "target_fps",
            "Target FPS must be finite and positive.");
    }
    _target_fps = value;
    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_window_settings(
    const elysia::config::WindowSettings& settings)
{
    if (!_window)
        return runtime_apply_failure("window_settings","Application window is unavailable.");

    const auto result = detail::apply_window_settings(settings,
        detail::ApplicationWindowOperations{
            .set_fullscreen = [this](std::uint32_t flags)
            {
                return SDL_SetWindowFullscreen(_window,flags != 0) ? 0 : -1;
            },
            .set_size = [this](int width,int height)
            {
                SDL_SetWindowSize(_window,width,height);
            },
            .center = [this]()
            {
                SDL_SetWindowPosition(
                    _window,
                    SDL_WINDOWPOS_CENTERED,
                    SDL_WINDOWPOS_CENTERED);
            },
            .error_message = []()
            {
                return std::string(SDL_GetError());
            }
        });
    if (!result)
    {
        return runtime_apply_failure(
            "window_settings",
            result.error());
    }
    return {};
}
}
