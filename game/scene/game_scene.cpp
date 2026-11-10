#include "game_scene.h"
#include "gameplay_setup.h"
#include "game/combat/player_fire.h"
#include "scene_keys.h"

#include "../characters/player_character.h"
#include "../characters/enemy.h"
#include "../map/dungeon_room.h"


#include "../../engine/tools/debug_draw.h"
#include "../../engine/camera/camera_manager.h"

#include "../combat/projectile_service.h"
#include "../combat/projectiles/bullet.h"
#include "../../engine/tools/logger.h"

GameScene::GameScene() : GameplayScene(game::room_features()) {}

void GameScene::on_enter(const elysia::scene::ScenePayload &payload)
{
    (void)payload;

    if (_player)
        return;

    resume();
    try
    {
        // bind projectile manager
        _projectiles.bind_scene(*this, physics_world());
        if (!ProjectileService::instance()->bind_manager(_projectiles))
            throw std::runtime_error("Cannot bind projectile service");

        _room = create_and_add_object<DungeonRoom>();
        if (!physics_world().set_tile_world(*_room))
            throw std::runtime_error("Cannot set tile world");

        // add player to scene
        _player = create_and_add_object<PlayerCharacter>(
            _room->center() - elysia::core::Vector2{32.0f, 32.0f});

        _player_controller = game::bind_player(control_context(), *_player);

        // enemy generator
        EnemyGenerator::Config enemy_config;
        enemy_config.difficulty = 5.0f;
        enemy_config.base_budget = 5.0f;
        const auto purchases = _enemy_generator.generate(enemy_config);
        const auto mapping = _enemy_spawn_layout.map(purchases.purchases, _room->center());
        for (const auto &spawn : mapping)
            create_and_add_object<Enemy>(spawn.position, spawn.definition);

        // config the camera
        constexpr auto slot = elysia::camera::CameraSlot::Main;
        camera_runtime().set_follow_strategy(slot, std::make_unique<elysia::camera::HardFollowStrategy>());
        camera_runtime().set_zoom(slot, 2.0f);
        camera_runtime().set_center(slot, _player->center());

        // config debug draw
        ELYSIA_DEBUG_DRAW->set_enabled(true);
        ELYSIA_DEBUG_DRAW->set_enabled_categories(elysia::tools::DebugDrawCategory::All);
    }
    catch (...)
    {
        clear_room();
        throw;
    }
}

void GameScene::on_exit()
{
    ELYSIA_DEBUG_DRAW->set_enabled(false);
    clear_room();
}

void GameScene::on_reset()
{
    clear_room();
}

void GameScene::on_before_update(double delta)
{
    // update schedule projectiles for fire
    if (!_paused)
        _projectiles.update(delta);
}

void GameScene::on_shortcuts(const elysia::input::RawInputFrame &input,
                         const std::vector<elysia::input::RawInputEvent> &events)
{
    (void)input;
    for (const elysia::input::RawInputEvent &event : events)
    {
        if (event.type == elysia::input::RawInputEventType::ControlPressed && event.control == elysia::input::RawInputControl::KeyEscape)
        {
            consume_input(event);
            request_scene_switch(MainMenu);
            return;
        }
    }
}

std::optional<elysia::camera::CameraFocus> GameScene::resolve_camera_focus(elysia::camera::CameraSlot slot) const
{
    return slot == elysia::camera::CameraSlot::Main && _player && !_player->is_destroyed()
               ? std::optional<elysia::camera::CameraFocus>{{_player->render_rect(), _player->render_rect()}}
               : std::nullopt;
}

void GameScene::clear_room() noexcept
{
    game::release_player(_player_controller);
    _combat.clear();
    _projectiles.unbind_scene();
    if (_room)
        (void)physics_world().clear_tile_world(*_room);
    static_cast<const elysia::object_query::IGameObjectQueryRuntime&>(*this)
        .visit_game_objects(elysia::core::DepthLayerMask::all(), [](auto& object) {
            object.destroy();
            return true;
        });
    _player = nullptr;
    _room = nullptr;
}

GameScene::~GameScene() noexcept { clear_room(); }
void GameScene::on_scene_object_registered(elysia::core::SceneObject &object)
{
    elysia::gameplay::GameplayScene::on_scene_object_registered(object);
    bool valid = true;
    if (auto *character = dynamic_cast<Character *>(&object); character && !character->is_dead())
        valid = _combat.register_character(*character);
    if (auto *bullet = dynamic_cast<Bullet *>(&object))
    {
        valid = _combat.register_bullet(*bullet);
        if (valid)
            bullet->initialize_fire();
    }
    if (!valid)
    {
        _combat.remove(object);
        object.destroy();
        throw std::runtime_error("Combat registration failed");
    }
}
void GameScene::on_control_target_removing(elysia::core::SceneObject &object)
{
    if (auto* projectile = dynamic_cast<Projectile*>(&object))
        projectile->detach_collision_listener();
    _combat.remove(object);

    if (&object == _player)
        _player = nullptr;
    if (&object == _room)
    {
        (void)physics_world().clear_tile_world(*_room);
        _room = nullptr;
    }
}

void GameScene::on_game_fixed_update(std::uint64_t, double)
{
    if (_player && !is_paused())
        game::fire_player_requests(*_player, camera(), _test_wand);
}
