#include "game_scene.h"

#include "../characters/player_character.h"
#include "../characters/enemy.h"
#include "../map/dungeon_room.h"

#include "scene_keys.h"

#include "../../engine/tools/debug_draw.h"
#include "../../engine/camera/camera_manager.h"

#include "../combat/projectile_service.h"
#include "../combat/projectiles/bullet.h"
#include "../../engine/tools/logger.h"

void GameScene::on_enter(const elysia::scene::ScenePayload &payload)
{
    (void)payload;

    if (_player)
        return;

    //bind projectile manager
    _projectiles.bind_scene(*this, physics_world());
    if (!ProjectileService::instance()->bind_manager(_projectiles))
        ELYSIA_LOG_ERROR("GameScene", "Cannot bind projectile service.");

    _room = create_and_add_object<DungeonRoom>();
    if(!physics_world().set_tile_world(*_room))
        ELYSIA_LOG_ERROR("GameScene", "Cannot set tile world.");

    //add player to scene
    _player = create_and_add_object<PlayerCharacter>(
        _room->center() - elysia::core::Vector2{32.0f, 32.0f});

    // test enemy
    _enemy = create_and_add_object<Enemy>(_room->center());

    //config the camera
    constexpr auto slot = elysia::camera::CameraSlot::Main;
    ELYSIA_CAMERA->set_follow_strategy(slot, std::make_unique<elysia::camera::HardFollowStrategy>());
    ELYSIA_CAMERA->set_zoom(slot, 2.0f);
    ELYSIA_CAMERA->set_center(slot, _player->center());

    //config debug draw
    ELYSIA_DEBUG_DRAW->set_enabled(true);
    ELYSIA_DEBUG_DRAW->set_enabled_categories(elysia::tools::DebugDrawCategory::All);
}

void GameScene::on_exit()
{
    ELYSIA_DEBUG_DRAW->set_enabled(false);
    clear_room();
}

void GameScene::reset()
{ 
    clear_room();
}

void GameScene::on_update(double delta)
{
    //update schedule projectiles for fire
    if (!_paused)
        _projectiles.update(delta);

    elysia::gameplay::GameplayScene::on_update(delta);
}

void GameScene::on_input(const elysia::input::RawInputFrame &input,
                         const std::vector<elysia::input::RawInputEvent> &events)
{
    //all tmp input handling
    elysia::gameplay::GameplayScene::on_input(input, events);
    for (const elysia::input::RawInputEvent &event : events)
    {
        if (event.type == elysia::input::RawInputEventType::ControlPressed && event.control == elysia::input::RawInputControl::KeyEscape)
        {
            request_scene_switch(MainMenu);
            return;
        }
        if (!_paused && event.type == elysia::input::RawInputEventType::ControlPressed
            && event.control == elysia::input::RawInputControl::KeyF)
        {
            fire_test_wand();
            return;
        }
    }
}

std::optional<elysia::core::Rect> GameScene::resolve_camera_focus_rect() const
{
    return _player && !_player->is_destroyed()
               ? std::optional<elysia::core::Rect>{_player->world_rect()}
               : std::nullopt;
}

//TODO: Refactor detailed lifecycle management in the next engine update.
void GameScene::clear_room() noexcept
{
    _combat.clear();
    std::vector<Character*> characters;
    std::vector<Projectile*> projectiles;
    static_cast<const elysia::object_query::IGameObjectQueryRuntime&>(*this)
        .visit_game_objects(elysia::core::DepthLayerMask::all(), [&](auto& object) {
            if (auto* character = dynamic_cast<Character*>(&object)) characters.push_back(character);
            if (auto* projectile = dynamic_cast<Projectile*>(&object)) projectiles.push_back(projectile);
            return true;
        });
    for (auto* character : characters) character->destroy();
    for (auto* projectile : projectiles) projectile->destroy();
    if (_enemy)
        _enemy->destroy();
    _enemy = nullptr;
    _projectiles.unbind_scene();
    if (_player)
        _player->destroy();
    if (_room)
    {
        (void)physics_world().clear_tile_world(*_room);
        _room->destroy();
    }
    _player = nullptr;
    _room = nullptr;
}

void GameScene::fire_test_wand()
{
    if (_paused || !_player || _player->is_destroyed() || _player->is_dead())
        return;

    const elysia::core::Vector2 direction{1.0f, 0.0f};
    if (!ProjectileService::instance()->request_fire({
            _player->actor_id(), _player->physics_handle(), _test_wand.attack(direction)}))
        ELYSIA_LOG_WARN("GameScene", "Projectile fire request rejected.");
}

GameScene::~GameScene() noexcept { clear_room(); }
//TODO: Refactor detailed lifecycle management in the next engine update.
//will be move down to engine next engine update
void GameScene::on_scene_object_registered(elysia::core::SceneObject& object)
{
    elysia::gameplay::GameplayScene::on_scene_object_registered(object);
    bool valid = true;
    if (auto* character = dynamic_cast<Character*>(&object)) valid = _combat.register_character(*character);
    if (auto* bullet = dynamic_cast<Bullet*>(&object))
    {
        valid = _combat.register_bullet(*bullet);
        if (valid) bullet->initialize_fire();
    }
    if (!valid)
    {
        _combat.remove(object);
        object.destroy();
        ELYSIA_LOG_ERROR("Combat", "Combat registration failed.");
    }
}
void GameScene::on_scene_object_removing(elysia::core::SceneObject& object)
{
    _combat.remove(object);
    elysia::gameplay::GameplayScene::on_scene_object_removing(object);
    if (&object == _player) _player = nullptr;
    if (&object == _enemy) _enemy = nullptr;
    if (&object == _room) _room = nullptr;
}
