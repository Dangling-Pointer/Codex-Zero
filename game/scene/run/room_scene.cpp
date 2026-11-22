#include "room_scene.h"
#include "room_transition.h"

#include "engine/tools/logger.h"
#include "game/characters/player_character.h"
#include "game/combat/player_fire.h"
#include "game/combat/projectile_service.h"
#include "game/combat/projectiles/bullet.h"
#include "game/gameflow/game_flow_manager.h"
#include "game/map/room_placement.h"
#include "game/objcets/portal.h"
#include "game/objcets/gate.h"
#include "game/scene/gameplay_setup.h"
#include "game/scene/scene_keys.h"
#include "game/ui/floor_minimap_hud.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
bool positive_dimensions(elysia::core::Vector2 size)
{
    return std::isfinite(size.x) && std::isfinite(size.y) && size.x >= 1 && size.y >= 1;
}

void validate_room_config(const RoomBuildConfig& config)
{
    const auto& map = config.map_config;

    // Match the physics world's tile-count limit before allocating a map.
    constexpr double max_room_tiles = 1'000'000;
    const double tile_count = static_cast<double>(config.grid_size.x) * config.grid_size.y;
    const bool valid_grid = positive_dimensions(config.grid_size) && tile_count <= max_room_tiles;
    const bool valid_room_sizes =
        positive_dimensions(map.baseRoomSize) && positive_dimensions(map.minSubRoomSize);
    if (!valid_grid || !valid_room_sizes || map.numSubRooms < 0 || map.minGap < 0)
        throw std::invalid_argument("Invalid room map configuration");
}
}

RoomScene::RoomScene() : GameplayScene(game::room_features()) {}
RoomScene::~RoomScene() noexcept { clear_room(); }

void RoomScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const auto* incoming = elysia::scene::try_scene_payload<RoomScenePayload>(payload);

    if (!incoming)
        throw std::invalid_argument("RoomScene requires RoomScenePayload");

    PlayerCharacter::validate_context(incoming->character);
    validate_room_config(incoming->room);
    validate_room_entry(*incoming);

    if (_active)
        return;

    _config = incoming->room;
    _room_id = incoming->room_id;
    _from_room = incoming->from_room;
    _corridor = incoming->corridor;
    const auto* room = run_room();
    _start_gate_delay = room && room->type == RoomType::Start && !_from_room ? 1.0 : 0.0;

    resume();

    try
    {
        _projectiles.bind_scene(*this, physics_world());
        if (!ProjectileService::instance()->bind_manager(_projectiles))
            throw std::runtime_error("Cannot bind room projectile service");

        build_room_objects(incoming->character);
        if (incoming->boundary_entry)
            game::restore_boundary_entry(*incoming->boundary_entry, _room.corridors,
                                         *_room.collision_world, *_player);
        create_room_content();
        if (should_create_gates())
            create_corridor_gates();

        game::initialize_room_camera(camera_runtime(), *_player, incoming->boundary_entry);

        if (_room_id || _corridor)
        {
            constexpr float size = 220, margin = 18;
            const float right = static_cast<float>(runtime_context().logical_width()) - margin;
            _minimap = create_and_add_object<FloorMinimapHud>(
                elysia::core::Rect{right - size, margin, size, size},
                GameFlowManager::instance()->current_floor());
            update_minimap();
        }

        _previous_player_center = _player->center();
        _active = true;
    }
    catch (...)
    {
        clear_room();
        throw;
    }
}

void RoomScene::on_exit() { clear_room(); }
void RoomScene::on_reset() { clear_room(); }

void RoomScene::build_room_objects(const CharacterContext& character)
{
    _room = build_room(_config);

    if (!_room.map || _room.map->is_destroyed())
        throw std::runtime_error("Room build did not provide a valid map");

    if (!_room.collision_world)
        throw std::runtime_error("Room build did not provide a tile collision world");

    const auto spawn = _room.player_spawn;
    if (!std::isfinite(spawn.x) || !std::isfinite(spawn.y))
        throw std::runtime_error("Room player spawn has non-finite coordinates");

    const auto& collision_world = *_room.collision_world;
    if (!physics_world().set_tile_world(collision_world))
        throw std::runtime_error("Cannot bind room tile collision world");

    const elysia::core::Rect player_bounds{spawn, game::kPlayerSpawnSize};
    if (!game::walkable_bounds(collision_world, player_bounds))
        throw std::runtime_error("Room player spawn must be on walkable ground");

    if (should_create_portal())
    {
        const auto portal_bounds = game::central_portal_bounds(*_room.map);
        if (!game::walkable_bounds(collision_world, portal_bounds))
            throw std::runtime_error("Room portal must be on walkable ground");
        if (portal_bounds.intersects(player_bounds))
            throw std::runtime_error("Room player spawn overlaps the portal");

        _portal = create_and_add_object<Portal>(portal_bounds);
        _portal->set_on_reached([this] { leave_through_portal(); });
    }

    _player = create_and_add_object<PlayerCharacter>(spawn, character);
    if (!_player || _player->is_destroyed())
        throw std::runtime_error("Cannot create room player");

    _player_controller = game::bind_player(control_context(), *_player);
}

void RoomScene::clear_room() noexcept
{
    _active = false;
    _transition_requested = false;
    _start_gate_delay = 0;
    _previous_player_center = {};
    if (_minimap)
        _minimap->destroy();
    _minimap = nullptr;
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
    _gates.clear();
    _room = {};
    _config = {};
    _room_id.reset();
    _from_room.reset();
    _corridor.reset();
    resume();
}

void RoomScene::on_before_update(double delta)
{
    // The first active update follows a successful SceneManager switch.
    if (_from_room)
    {
        auto& flow = *GameFlowManager::instance();
        const bool returning = _corridor && _room_id == _corridor->from;
        if (flow.get_current_room_id() != *_from_room || (!returning && !flow.try_enter_room(*_room_id)))
            throw std::logic_error("Dungeon room entry is no longer valid");
        _from_room.reset();
    }

    if (_active && !_paused)
    {
        _start_gate_delay = std::max(0.0, _start_gate_delay - std::max(0.0, delta));
        update_gates();
        _projectiles.update(delta);
    }
}

void RoomScene::create_corridor_gates()
{
    const auto tile = _room.collision_world->tile_size();
    const bool raised = _start_gate_delay > 0 || !can_leave_room();
    for (const auto& corridor : _room.corridors)
    {
        const auto strip = corridor.gate_strip(tile);
        for (float y = strip.top(); y < strip.bottom(); y += tile.y)
            for (float x = strip.left(); x < strip.right(); x += tile.x)
                _gates.push_back(create_and_add_object<Gate>(elysia::core::Vector2{x, y}, raised));
    }
}

void RoomScene::update_gates()
{
    if (_gates.empty())
        return;
    const bool raised = _start_gate_delay > 0 || !can_leave_room();
    for (auto* gate : _gates)
        if (!gate->is_destroyed())
            gate->set_raised(raised);
}

void RoomScene::on_after_update(double)
{
    update_minimap();
    if (!_active || is_paused() || !_room.map || _room.map->is_destroyed())
        return;
    if (_transition_requested || !_player || _player->is_destroyed() || !_player->is_active() || _player->is_dead())
        return;

    const bool leaving_allowed = can_leave_room();
    if (leaving_allowed && try_leave_through_boundary())
        return;
    _previous_player_center = _player->center();

    if (_portal && leaving_allowed)
        _portal->check_reached(_player->world_rect());
}

void RoomScene::update_minimap()
{
    if (!_minimap || _minimap->is_destroyed() || !_player || _player->is_destroyed())
        return;
    if (_room_id)
        _minimap->set_player_location(*_room_id, _player->render_rect().center(), _room.corridors);
    else if (_corridor)
    {
        const auto layout = game::make_corridor_layout(GameFlowManager::instance()->current_floor(), *_corridor);
        const float progress = layout.progress(_player->render_rect().center(),
            _room.collision_world->world_origin(), _room.collision_world->tile_size());
        _minimap->set_corridor_location(*_corridor, progress);
    }
}

bool RoomScene::try_leave_through_boundary()
{
    std::optional<elysia::scene::SceneRoute> route;
    if (_room_id)
        route = game::find_boundary_route(*_room_id, _room.corridors, _previous_player_center, *_player, camera());
    else if (_corridor)
        route = game::find_corridor_exit_route(*_corridor, _room.corridors, _previous_player_center, *_player, camera());
    if (!route)
        return false;

    request_scene_switch(*route);
    _transition_requested = true;
    return true;
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
    if (!_player || _player->is_destroyed())
        return std::nullopt;

    return _player->capture_context();
}

std::optional<elysia::camera::CameraFocus> RoomScene::resolve_camera_focus(
    elysia::camera::CameraSlot slot) const
{
    if (slot != elysia::camera::CameraSlot::Main || !_player || _player->is_destroyed())
        return std::nullopt;

    const auto bounds = _player->render_rect();
    return elysia::camera::CameraFocus{bounds, bounds};
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
        if (valid)
            bullet->initialize_fire();
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

    if (&object == _player)
        _player = nullptr;
    if (&object == _portal)
        _portal = nullptr;
    if (&object == _minimap)
        _minimap = nullptr;
    if (auto* gate = dynamic_cast<Gate*>(&object))
        std::erase(_gates, gate);
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
    {
        if (event.type != elysia::input::RawInputEventType::ControlPressed
            || event.control != elysia::input::RawInputControl::KeyEscape)
            continue;

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
