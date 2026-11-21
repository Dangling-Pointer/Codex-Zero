#include "scene_harness.h"
#include "game/scene/run/fixed_room_scene.h"
#include "game/scene/scene_keys.h"
#include "game/gameflow/game_flow_manager.h"
#include "game/map/dungeon_room.h"
#include "game/map/room_placement.h"
#include "game/characters/character_manager.h"
#include "game/characters/player_character.h"
#include "game/objcets/portal.h"
#include "engine/object_query/game_object_query_service.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
using elysia::core::Vector2;
using elysia::core::Rect;
using Key = elysia::input::RawInputControl;

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool near(Vector2 first, Vector2 second, float tolerance = 0.15f)
{
    return first.distance_squared_to(second) < tolerance * tolerance;
}

game::RoomCorridorLayout corridor(const DungeonRoom& map, RoomDirection direction)
{
    for (const auto& exit : map.corridors())
        if (exit.direction == direction) return exit;
    throw std::runtime_error("Expected corridor is missing");
}

Key direction_key(RoomDirection direction)
{
    switch (direction)
    {
    case RoomDirection::Up: return Key::KeyW;
    case RoomDirection::Right: return Key::KeyD;
    case RoomDirection::Down: return Key::KeyS;
    case RoomDirection::Left: return Key::KeyA;
    default: throw std::runtime_error("Invalid test direction");
    }
}

void geometry()
{
    for (const Vector2 body : {Vector2{15, 15}, Vector2{12, 10}, Vector2{9, 8}})
    {
        MapConfig config;
        config.baseRoomSize = body;
        config.numSubRooms = 0;
        config.corridors.directions = RoomDirection::All;
        DungeonRoom map(config, body + config.corridors.padding() * 2);
        map.set_position({96, -160});
        check(map.corridors().size() == 4, "four directions produce four layouts");
        for (const auto& exit : map.corridors())
        {
            const auto normal = game::corridor_normal(exit.direction);
            const bool vertical = normal.x == 0;
            check((vertical ? exit.walkable.width() : exit.walkable.height()) == config.corridors.width * 32,
                  "net width excludes both side walls");
            const int length = vertical ? config.corridors.vertical_length : config.corridors.horizontal_length;
            check((vertical ? exit.walkable.height() : exit.walkable.width()) == length * 32,
                  "net strip uses the directional length");
            check(game::walkable_bounds(map, Rect::from_center(exit.seam_center(), {32, 32})),
                  "world-space midpoint lies on walkable ground with a translated map");

            const auto previous = exit.seam_center() - normal * 100;
            const auto current = exit.seam_center() + normal * 20;
            check(exit.crossed_outward(previous, current, Rect::from_center(current, {20, 12})),
                  "swept detection catches a fast outward crossing");
            check(!exit.crossed_outward(current, previous, Rect::from_center(previous, {20, 12})),
                  "inward entry cannot bounce back");
            check(exit.crossed_outward(exit.seam_center(), current, Rect::from_center(current, {20, 12})),
                  "leaving an exact midpoint is permitted");
            check(!exit.crossed_outward(exit.seam_center(), exit.seam_center(),
                                        Rect::from_center(exit.seam_center(), {20, 12})),
                  "standing on the midpoint does not switch");
            const auto tangent = vertical ? Vector2{1, 0} : Vector2{0, 1};
            check(!exit.crossed_outward(previous + tangent * 160, current + tangent * 160,
                                        Rect::from_center(current + tangent * 160, {20, 12})),
                  "crossings outside the corridor are ignored");
        }
    }

    MapConfig disabled;
    disabled.numSubRooms = 0;
    DungeonRoom map(disabled, {14, 14});
    check(map.corridors().empty(), "ordinary rectangle has no physical exits");
    disabled.corridors.directions = RoomDirection::All;
    bool rejected = false;
    try { DungeonRoom invalid(disabled, {14, 14}); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "missing corridor padding is rejected");
}

struct BoundaryObservation
{
    RoomScene* scene = nullptr;
    int builds = 0;
    Vector2 center;
    PlayerMotionState motion;
    Vector2 camera_offset;
    float zoom = 2;
};

class ObservedBoundaryRoom : public FixedRoomScene
{
public:
    explicit ObservedBoundaryRoom(BoundaryObservation& observation) : _observation(observation)
    {
        _observation.scene = this;
    }
    void on_after_update(double delta) override
    {
        RoomScene::on_after_update(delta);
        if (player())
        {
            _observation.center = player()->center();
            _observation.motion = player()->capture_motion();
            _observation.camera_offset = camera().center() - player()->render_rect().center();
            _observation.zoom = camera().zoom();
        }
    }

protected:
    RoomBuildResult build_room(const RoomBuildConfig& config) override
    {
        ++_observation.builds;
        return FixedRoomScene::build_room(config);
    }

private:
    BoundaryObservation& _observation;
};

class BoundaryTestRun
{
    elysia::io::ContentRegistry _content;
    elysia::scene::SceneRuntimeContext _context{nullptr, _content, 1280, 720};
    elysia::input::RawInputState _keyboard;
public:
    BoundaryObservation observation;
    elysia::scene::SceneManager manager;

    BoundaryTestRun()
    {
        manager.initialize(_context);
        auto* controllers = elysia::gameplay::ControllerService::instance();
        if (!controllers->session_active()) check(controllers->begin_session().has_value(), "start controller session");
        manager.register_game_scene<ObservedBoundaryRoom>(FixedRoom, std::ref(observation));
        manager.register_game_scene<test::EmptyScene>(MainMenu);
        manager.register_game_scene<test::EmptyScene>(FloorMap);
        auto& flow = *GameFlowManager::instance();
        flow.restart(42);
        const auto character = *CharacterManager::instance()->create_context("default_player");
        manager.start(make_run_room_route(0, character));
        update(0);
    }

    PlayerCharacter& player() { return *ELYSIA_OBJECT_QUERY->find_object<PlayerCharacter>(); }
    DungeonRoom& map() { return *ELYSIA_OBJECT_QUERY->find_object<DungeonRoom>(); }
    void update(double delta)
    {
        manager.on_update(delta);
        check(manager.state() == elysia::scene::SceneManagerState::Running, "boundary scene stays running");
        check(manager.current_scene_key() == FixedRoom, "boundary switch stays in fixed rooms");
    }

    void keys(std::initializer_list<Key> held)
    {
        using namespace elysia::input;
        InputSnapshot input;
        InputSourceFrame source{.source = InputSourceId::keyboard(), .initial_state = _keyboard};
        RawInputState next;
        for (auto key : held) next.set_pressed(key, true);
        for (auto key : {Key::KeyW, Key::KeyD, Key::KeyS, Key::KeyA})
        {
            if (_keyboard.is_pressed(key) == next.is_pressed(key)) continue;
            input.events.push_back({.control = key,
                .type = next.is_pressed(key) ? RawInputEventType::ControlPressed : RawInputEventType::ControlReleased,
                .device = InputDevice::Keyboard, .source = InputSourceId::keyboard()});
        }
        _keyboard = next;
        source.frame.state = _keyboard;
        input.sources.push_back(source);
        manager.on_input(input);
    }

    void cross(RoomDirection direction, float lateral = 13, bool diagonal = false)
    {
        auto& flow = *GameFlowManager::instance();
        const RoomID from = flow.get_current_room_id();
        const auto* next = flow.connected_room(from, direction);
        check(next, "source has a real reciprocal connection");
        const RoomID to = next->id;
        const auto exit = corridor(map(), direction);
        const auto normal = game::corridor_normal(direction);
        const auto tangent = normal.x == 0 ? Vector2{1, 0} : Vector2{0, 1};
        keys({});
        const auto start = exit.seam_center() - normal * 1 + tangent * lateral;
        check(player().physics_world()->teleport_object(player().physics_handle(), start - player().size() * 0.5f),
              "position source just inside its midpoint");
        update(0); // Establish previous position without a physics step.
        const int builds = observation.builds;
        if (diagonal) keys({direction_key(direction), normal.x == 0 ? Key::KeyD : Key::KeyS});
        else keys({direction_key(direction)});
        for (int frame = 0; frame < 5 && observation.builds == builds; ++frame)
            update(1.0 / 60.0);
        check(observation.builds == builds + 1, "one outward crossing rebuilds the shared scene once");

        const auto entry = corridor(map(), game::opposite_direction(direction));
        check(near(player().center() - entry.seam_center(), observation.center - exit.seam_center()),
              "destination retains lateral offset and overshoot in world axes");
        check(near(player().velocity(), observation.motion.velocity), "velocity survives the first rendered frame");
        check(near(player().capture_motion().direction, observation.motion.direction), "direction survives entry");
        check(near(player().capture_motion().render_offset, observation.motion.render_offset),
              "physics presentation offset survives entry");
        check(near(observation.scene->camera().center() - player().render_rect().center(), observation.camera_offset)
            && observation.scene->camera().zoom() == observation.zoom,
              "camera keeps the player's screen position and zoom on the entry frame");
        check(flow.get_current_room_id() == from, "floor is committed only after scene construction succeeds");
        check(player().physics_world()->registered_object_count() == 1
            && ELYSIA_OBJECT_QUERY->find_objects<PlayerCharacter>().size() == 1,
              "same-key reuse has one live player and physics body");

        update(0);
        check(flow.get_current_room_id() == to && flow.find_room(to)->visited && flow.find_room(to)->discovered,
              "successful boundary entry commits visit state");
        const auto before = player().center();
        if (diagonal) keys({direction_key(direction), normal.x == 0 ? Key::KeyD : Key::KeyS});
        else keys({direction_key(direction)}); // No new press event: the key is still held.
        update(1.0 / 60.0);
        const auto moved = player().center() - before;
        check(moved.x * normal.x + moved.y * normal.y > 1, "held movement continues without another key press");
        check(observation.builds == builds + 1, "inward movement never bounces to the source");
        keys({});
        update(1.0 / 60.0);
        check(player().velocity().is_zero(), "releasing a transferred key stops movement");
    }
};

void invalid_entries()
{
    for (int scenario = 0; scenario < 5; ++scenario)
    {
        BoundaryTestRun run;
        auto& flow = *GameFlowManager::instance();
        auto route = make_boundary_room_route(0, RoomDirection::Right, run.player().capture_context(), {});
        auto payload = *elysia::scene::try_scene_payload<RoomScenePayload>(route.payload);
        const RoomID destination = *payload.room_id;
        switch (scenario)
        {
        case 0: payload.boundary_entry->direction = RoomDirection::Up; break;
        case 1: payload.room.map_config.corridors.directions = RoomDirection::None; break;
        case 2: payload.boundary_entry->seam_offset = {0, 500}; break;
        case 3: payload.boundary_entry->motion.velocity.x = std::numeric_limits<float>::quiet_NaN(); break;
        case 4: payload.from_room = 4; break;
        }
        route.payload = payload;
        run.manager.on_scene_request({elysia::scene::SceneRequestType::Switch, route});
        run.manager.on_update(0);
        check(flow.get_current_room_id() == 0 && !flow.find_room(destination)->visited,
              "malformed or unbuildable boundary entry never commits progression");
    }
}

void exact_midpoint_and_reverse()
{
    BoundaryTestRun run;
    auto& flow = *GameFlowManager::instance();
    const auto character = run.player().capture_context();
    const auto route = make_boundary_room_route(0, RoomDirection::Up, character,
        {.motion = {.direction = {0, -1}, .velocity = {0, -200}}});
    const RoomID target = flow.connected_room(0, RoomDirection::Up)->id;
    run.manager.on_scene_request({elysia::scene::SceneRequestType::Switch, route});
    run.update(0);
    run.update(0);
    check(flow.get_current_room_id() == target, "exact seam entry commits without bouncing");
    const int builds = run.observation.builds;
    run.keys({Key::KeyS}); // Immediate reversal on the seam, with no cooldown.
    run.update(1.0 / 60.0);
    run.update(0);
    check(flow.get_current_room_id() == 0 && run.observation.builds == builds + 1,
          "an exact midpoint entry can immediately turn back");
}

void boundary_transitions()
{
    BoundaryTestRun run;
    auto& flow = *GameFlowManager::instance();
    check(!flow.connected_room(0, RoomDirection::All)
        && !flow.connected_room(std::numeric_limits<RoomID>::max(), RoomDirection::Up),
          "invalid IDs and combined direction masks are not physical exits");
    for (auto direction : {RoomDirection::Up, RoomDirection::Right, RoomDirection::Down, RoomDirection::Left})
    {
        run.cross(direction);
        run.cross(game::opposite_direction(direction), -17);
        check(flow.get_current_room_id() == 0, "all four differently sized room pairs support a return trip");
    }
    run.cross(RoomDirection::Right, 61, true); // Near a side wall; sprite may overhang it.
    run.cross(RoomDirection::Left, -62);

    // Traverse every edge of the generated tree, including Event -> Event and gap connections.
    bool crossed_gap = false;
    const auto traverse = [&](auto&& self, RoomID parent) -> void {
        const RoomID from = flow.get_current_room_id();
        for (auto direction : {RoomDirection::Up, RoomDirection::Right, RoomDirection::Down, RoomDirection::Left})
        {
            const auto* next = flow.connected_room(from, direction);
            if (!next || next->id == parent) continue;
            const auto first = *flow.current_floor().find_position(from);
            const auto second = *flow.current_floor().find_position(next->id);
            crossed_gap |= std::abs(first.x - second.x) + std::abs(first.y - second.y) > 1;
            run.cross(direction);
            self(self, from);
            run.cross(game::opposite_direction(direction));
        }
    };
    traverse(traverse, std::numeric_limits<RoomID>::max());
    check(crossed_gap && flow.get_current_room_id() == 0, "real gap links route directly and return to Start");

    const auto character = run.player().capture_context();
    bool rejected = false;
    try { (void)make_boundary_room_route(0, RoomDirection::All, character, {}); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected && flow.get_current_room_id() == 0, "invalid boundary request cannot alter progression");
}
}

void fixed_room_corridor_tests()
{
    geometry();
    boundary_transitions();
    exact_midpoint_and_reverse();
    invalid_entries();
}
