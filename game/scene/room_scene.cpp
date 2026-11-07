#include "room_scene.h"

#include "game/characters/player_character.h"
#include "game/combat/projectile_service.h"
#include "game/combat/projectiles/bullet.h"

#include <cmath>
#include <stdexcept>

namespace
{
bool positive_dimensions(elysia::core::Vector2 size)
{
    return std::isfinite(size.x) && std::isfinite(size.y) && size.x >= 1 && size.y >= 1;
}
}

void RoomScene::validate_payload(const RoomScenePayload& payload)
{
    PlayerCharacter::validate_context(payload.character);
    const auto& config = payload.room;
    // Match the physics world's tile-count limit before allocating a map.
    if (!positive_dimensions(config.grid_size)
        || double(config.grid_size.x) * config.grid_size.y > 1000000
        || !positive_dimensions(config.map_config.baseRoomSize)
        || !positive_dimensions(config.map_config.minSubRoomSize)
        || config.map_config.numSubRooms < 0 || config.map_config.minGap < 0)
        throw std::invalid_argument("Invalid room map configuration");
}

void RoomScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const auto* incoming = elysia::scene::try_scene_payload<RoomScenePayload>(payload);
    if (!incoming)
        throw std::invalid_argument("RoomScene requires RoomScenePayload");
    validate_payload(*incoming);
    if (_active)
        return;

    _config = incoming->room;
    _paused = false;
    try
    {
        _projectiles.bind_scene(*this, physics_world());
        if (!ProjectileService::instance()->bind_manager(_projectiles))
            throw std::runtime_error("Cannot bind room projectile service");
        _room = build_room(_config);
        if (!_room.map || _room.map->is_destroyed() || !_room.collision_world
            || !std::isfinite(_room.player_spawn.x) || !std::isfinite(_room.player_spawn.y)
            || !physics_world().set_tile_world(*_room.collision_world))
            throw std::runtime_error("Cannot build room collision world");

        _player = create_and_add_object<PlayerCharacter>(_room.player_spawn, incoming->character);
        if (!_player || _player->is_destroyed())
            throw std::runtime_error("Cannot create room player");
        create_room_content();
        constexpr auto slot = elysia::camera::CameraSlot::Main;
        auto* camera = elysia::camera::CameraManager::instance();
        camera->set_follow_strategy(slot, std::make_unique<elysia::camera::HardFollowStrategy>());
        camera->set_zoom(slot, 2.0f);
        camera->set_center(slot, _player->center());
        _active = true;
    }
    catch (...)
    {
        clear_room();
        throw;
    }
}

RoomScene::~RoomScene() noexcept { clear_room(); }
void RoomScene::on_exit() { clear_room(); }
void RoomScene::reset() { clear_room(); }

void RoomScene::clear_room() noexcept
{
    _active = false;
    _combat.clear();
    _projectiles.unbind_scene();
    if (_room.collision_world)
        (void)physics_world().clear_tile_world(*_room.collision_world);
    static_cast<const elysia::object_query::IGameObjectQueryRuntime&>(*this)
        .visit_game_objects(elysia::core::DepthLayerMask::all(), [](auto& object) {
            object.destroy();
            return true;
        });
    _player = nullptr;
    _room = {};
    _config = {};
    _paused = false;
}

void RoomScene::on_update(double delta)
{
    if (_active && !_paused)
        _projectiles.update(delta);
    elysia::gameplay::GameplayScene::on_update(delta);
}

std::optional<CharacterContext> RoomScene::capture_character_context() const
{
    return _player && !_player->is_destroyed()
        ? std::optional<CharacterContext>{_player->capture_context()} : std::nullopt;
}

std::optional<elysia::core::Rect> RoomScene::resolve_camera_focus_rect() const
{
    return _player && !_player->is_destroyed()
        ? std::optional<elysia::core::Rect>{_player->world_rect()} : std::nullopt;
}

void RoomScene::on_scene_object_registered(elysia::core::SceneObject& object)
{
    elysia::gameplay::GameplayScene::on_scene_object_registered(object);
    bool valid = true;
    if (auto* character = dynamic_cast<Character*>(&object); character && !character->is_dead())
        valid = _combat.register_character(*character);
    if (auto* bullet = dynamic_cast<Bullet*>(&object))
    {
        valid = _combat.register_bullet(*bullet);
        if (valid) bullet->initialize_fire();
    }
    if (!valid)
    {
        _combat.remove(object);
        object.destroy();
        throw std::runtime_error("Room combat registration failed");
    }
}

void RoomScene::on_scene_object_removing(elysia::core::SceneObject& object)
{
    _combat.remove(object);
    elysia::gameplay::GameplayScene::on_scene_object_removing(object);
    if (&object == _player) _player = nullptr;
    if (&object == _room.map)
    {
        if (_room.collision_world)
            (void)physics_world().clear_tile_world(*_room.collision_world);
        _room = {};
    }
}
