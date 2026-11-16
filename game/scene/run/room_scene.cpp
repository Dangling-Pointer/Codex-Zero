#include "room_scene.h"
#include "game/scene/gameplay_setup.h"
#include "game/combat/player_fire.h"
#include "game/scene/scene_keys.h"
#include "game/gameflow/game_flow_manager.h"

#include "game/characters/player_character.h"
#include "game/map/room_placement.h"
#include "game/objcets/portal.h"
#include "engine/tools/logger.h"
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

    if (payload.room_id)
    {
        const auto& flow = *GameFlowManager::instance();
        const auto* room = flow.find_room(*payload.room_id);
        const auto current = flow.get_current_room_id();
        const bool valid = payload.from_room
            ? *payload.from_room == current && flow.can_enter_room(*payload.room_id)
            : room && room->type == RoomType::Start && room->id == current;
        if (!valid)
            throw std::invalid_argument("Invalid dungeon room entry");
    }
    else if (payload.from_room)
        throw std::invalid_argument("Dungeon room entry requires a destination");
}

RoomScene::RoomScene() : GameplayScene(game::room_features()) {}

void RoomScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const auto* incoming = elysia::scene::try_scene_payload<RoomScenePayload>(payload);
    if (!incoming)
        throw std::invalid_argument("RoomScene requires RoomScenePayload");
    validate_payload(*incoming);
    if (_active)
        return;

    _config = incoming->room;
    _room_id = incoming->room_id;
    _from_room = incoming->from_room;
    resume();
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

        const auto portal_bounds = game::central_portal_bounds(*_room.map);
        const elysia::core::Rect player_bounds{_room.player_spawn, game::kPlayerSpawnSize};
        if (!game::walkable_bounds(*_room.collision_world, portal_bounds)
            || !game::walkable_bounds(*_room.collision_world, player_bounds)
            || portal_bounds.intersects(player_bounds))
            throw std::runtime_error("Room requires a walkable central portal and separate player spawn");
        _portal = create_and_add_object<Portal>(portal_bounds);
        _portal->set_on_reached([this] { leave_through_portal(); });

        _player = create_and_add_object<PlayerCharacter>(_room.player_spawn, incoming->character);
        if (!_player || _player->is_destroyed())
            throw std::runtime_error("Cannot create room player");
        _player_controller = game::bind_player(control_context(), *_player);
        create_room_content();
        constexpr auto slot = elysia::camera::CameraSlot::Main;
        camera_runtime().set_follow_strategy(slot, std::make_unique<elysia::camera::HardFollowStrategy>());
        camera_runtime().set_zoom(slot, 2.0f);
        camera_runtime().set_center(slot, _player->center());
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
void RoomScene::on_reset() { clear_room(); }

void RoomScene::clear_room() noexcept
{
    _active = false;
    game::release_player(_player_controller);
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
    _portal = nullptr;
    _room = {};
    _config = {};
    _room_id.reset();
    _from_room.reset();
    resume();
}

void RoomScene::on_before_update(double delta)
{
    // The first active update follows a successful SceneManager switch.
    if (_from_room)
    {
        auto& flow = *GameFlowManager::instance();
        if (flow.get_current_room_id() != *_from_room || !flow.try_enter_room(*_room_id))
            throw std::logic_error("Dungeon room entry is no longer valid");
        _from_room.reset();
    }
    if (_active && !_paused)
        _projectiles.update(delta);
}

void RoomScene::on_after_update(double)
{
    if (_active && _room.map && !_room.map->is_destroyed() && !is_paused() && _portal && _player
        && !_player->is_destroyed() && _player->is_active() && !_player->is_dead()
        && can_leave_room())
        _portal->check_reached(_player->world_rect());
}

const Room* RoomScene::run_room() const
{
    return _room_id ? GameFlowManager::instance()->find_room(*_room_id) : nullptr;
}

void RoomScene::leave_through_portal()
{
    const auto* room = run_room();
    if (!room)
    {
        ELYSIA_LOG_INFO("gameplay", "Room portal reached");
        return;
    }
    if (room->type == RoomType::End)
    {
        request_scene_switch(MainMenu);
        return;
    }

    const auto character = capture_character_context();
    if (!character)
        throw std::logic_error("Cannot leave a dungeon room without a player");

    request_scene_switch(FloorMap, FloorMapPayload{room->id, *character});
    if (room->type == RoomType::Combat || room->type == RoomType::Elite)
        GameFlowManager::instance()->mark_current_room_cleared();
}

std::optional<CharacterContext> RoomScene::capture_character_context() const
{
    return _player && !_player->is_destroyed()
        ? std::optional<CharacterContext>{_player->capture_context()} : std::nullopt;
}

std::optional<elysia::camera::CameraFocus> RoomScene::resolve_camera_focus(elysia::camera::CameraSlot slot) const
{
    return slot == elysia::camera::CameraSlot::Main && _player && !_player->is_destroyed()
        ? std::optional<elysia::camera::CameraFocus>{{_player->render_rect(), _player->render_rect()}} : std::nullopt;
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

void RoomScene::on_control_target_removing(elysia::core::SceneObject& object)
{
    if (auto* projectile = dynamic_cast<Projectile*>(&object))
        projectile->detach_collision_listener();
    _combat.remove(object);

    if (&object == _player) _player = nullptr;
    if (&object == _portal) _portal = nullptr;
    if (&object == _room.map)
    {
        if (_room.collision_world)
            (void)physics_world().clear_tile_world(*_room.collision_world);
        _room = {};
    }
}

void RoomScene::on_shortcuts(const elysia::input::RawInputFrame&,
    const std::vector<elysia::input::RawInputEvent>& events)
{
    for (const auto& event : events)
        if (event.type == elysia::input::RawInputEventType::ControlPressed
            && event.control == elysia::input::RawInputControl::KeyEscape)
        {
            consume_input(event);
            request_scene_switch(MainMenu);
            return;
        }
}

void RoomScene::on_game_fixed_update(std::uint64_t, double)
{
    if (_player && !is_paused())
        game::fire_player_requests(*_player, camera(), _test_wand);
}
