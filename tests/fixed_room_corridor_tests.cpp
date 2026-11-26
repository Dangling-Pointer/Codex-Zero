#include "scene_harness.h"
#include "game/map/room_service.h"
#include "room_test_helpers.h"
#include "game/scene/run/fixed_room_scene.h"
#include "game/scene/run/corridor_scene.h"
#include "game/scene/scene_keys.h"
#include "game/gameflow/game_flow_manager.h"
#include "game/map/dungeon_room.h"
#include "room_test_helpers.h"
#include "game/characters/character_manager.h"
#include "game/characters/player_character.h"
#include "game/characters/enemy.h"
#include "game/combat/projectiles/bullet.h"
#include "game/objcets/portal.h"
#include "game/objcets/gate.h"
#include "game/ui/floor_minimap_hud.h"
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

std::vector<Vector2> hud_rooms(const FloorMinimapHud& hud)
{
    std::vector<elysia::core::UiRenderCommand> commands;
    hud.submit_ui_render_commands(commands);
    std::vector<Vector2> rooms;
    for (const auto& command : commands)
        if (command.type == elysia::core::UiRenderCommandType::FillRoundedRect && command.corner_radius == 3)
            rooms.push_back(command.screen_rect.center());
    return rooms;
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
            check(test::walkable(map, Rect::from_center(exit.seam_center(), {32, 32})),
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
    FloorMinimapHud* hud = nullptr;
    int active_huds = 0;
    bool room_locked = false;
    int builds = 0;
    Vector2 center;
    PlayerMotionState motion;
    Vector2 camera_offset;
    float zoom = 2;
    std::vector<Vector2> hud_positions;
};

template<class Base> class ObservedBoundaryRoom : public Base
{
public:
    explicit ObservedBoundaryRoom(BoundaryObservation& observation) : _observation(observation)
    {
        _observation.scene = this;
    }
    void on_after_update(double delta) override
    {
        RoomScene::on_after_update(delta);
        if (this->player())
        {
            _observation.center = this->player()->center();
            _observation.motion = this->player()->capture_motion();
            _observation.camera_offset = this->camera().center() - this->player()->render_rect().center();
            _observation.zoom = this->camera().zoom();
            if (_observation.hud)
                _observation.hud_positions = hud_rooms(*_observation.hud);
        }
    }

protected:
    bool can_leave_room() const override { return !_observation.room_locked; }

    void on_scene_object_registered(elysia::core::SceneObject& object) override
    {
        RoomScene::on_scene_object_registered(object);
        if (auto* hud = dynamic_cast<FloorMinimapHud*>(&object))
        {
            _observation.hud = hud;
            ++_observation.active_huds;
        }
    }

    void on_control_target_removing(elysia::core::SceneObject& object) override
    {
        if (dynamic_cast<FloorMinimapHud*>(&object))
            --_observation.active_huds;
        if (&object == _observation.hud)
            _observation.hud = nullptr;
        RoomScene::on_control_target_removing(object);
    }

    RoomBuildResult build_room(const RoomBuildConfig& config) override
    {
        _observation.scene = this;
        ++_observation.builds;
        return Base::build_room(config);
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
        manager.register_game_scene<ObservedBoundaryRoom<FixedRoomScene>>(FixedRoom, std::ref(observation));
        manager.register_game_scene<ObservedBoundaryRoom<CorridorScene>>(Corridor, std::ref(observation));
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
        check(manager.current_scene_key() == FixedRoom || manager.current_scene_key() == Corridor,
              "physical traversal stays in rooms or their connecting corridors");
        const auto* query = game::RoomService::instance();
        check(query && query->map_bounds() == map().world_rect(),
              "boundary switches publish a query for the new map");
        if (manager.current_scene_key() == Corridor)
            check(query->region_at(player().center()).region == game::RoomRegion::Corridor,
                  "gap corridor query reports passage even between its two exits");
    }

    void keys(std::initializer_list<Key> held)
    {
        using namespace elysia::input;
        InputSnapshot input;
        InputSourceFrame source{.source = InputSourceId::keyboard(), .initial_state = _keyboard};
        RawInputState next;
        for (auto key : held) next.set_pressed(key, true);
        for (auto key : {Key::KeyW, Key::KeyD, Key::KeyS, Key::KeyA, Key::KeyF})
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
        auto exit = corridor(map(), direction);
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

        int expected_builds = builds + 1;
        const int gap = game::connection_gap(flow.current_floor(), from, to);
        if (gap > 0)
        {
            check(manager.current_scene_key() == Corridor, "a gap enters the formal corridor scene");
            const auto target_before = *flow.find_room(to);
            check(flow.get_current_room_id() == from, "entering a corridor keeps the source room current");
            check(ELYSIA_OBJECT_QUERY->find_objects<Gate>().empty()
                && ELYSIA_OBJECT_QUERY->find_objects<Portal>().empty()
                && ELYSIA_OBJECT_QUERY->find_objects<Enemy>().empty()
                && player().physics_world()->registered_object_count() == 1,
                  "formal corridor contains only its map and player");
            const auto layout = game::make_corridor_layout(flow.current_floor(), {from, to, direction});
            const auto ends = layout.world_exits(map().world_origin(), map().tile_size());
            check(near(player().center() - ends.front().seam_center(), observation.center - exit.seam_center()),
                  "room-to-corridor entry preserves lateral offset and overshoot");
            check(near(player().velocity(), observation.motion.velocity), "corridor entry preserves velocity");
            check(near(hud_rooms(*observation.hud).front(), observation.hud_positions.front(), 1.0f),
                  "room-to-corridor HUD projection is continuous on the entry frame");
            update(0);
            for (int frame = 0; frame < 600 && observation.builds == builds + 1; ++frame)
            {
                if (diagonal) keys({direction_key(direction), normal.x == 0 ? Key::KeyD : Key::KeyS});
                else keys({direction_key(direction)});
                update(1.0 / 60.0);
                if (manager.current_scene_key() == Corridor)
                    check(flow.get_current_room_id() == from && flow.find_room(to)->visited == target_before.visited
                        && flow.find_room(to)->discovered == target_before.discovered,
                          "corridor movement never commits target exploration early");
            }
            ++expected_builds;
            exit = ends.back();
            check(observation.builds == expected_builds && manager.current_scene_key() == FixedRoom,
                  "walking the added distance reaches the target room");
        }
        else
            check(manager.current_scene_key() == FixedRoom, "adjacent rooms still switch directly");

        const auto entry = corridor(map(), game::opposite_direction(direction));
        check(near(hud_rooms(*observation.hud).front(), observation.hud_positions.front(), 1.0f),
              "room entry preserves HUD projection across direct and corridor transitions");
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
        const auto gates = ELYSIA_OBJECT_QUERY->find_objects<Gate>();
        check(player().physics_world()->registered_object_count() == 1 + gates.size()
            && ELYSIA_OBJECT_QUERY->find_objects<PlayerCharacter>().size() == 1,
              "same-key reuse keeps one player and exactly the corridor gate bodies");
        check(gates.size() == map().corridors().size() * kStandardRoomCorridors.width
            && std::ranges::none_of(gates, &Gate::is_raised), "Event room gate rows are lowered and traversable");
        check(observation.active_huds == 1 && observation.hud && !observation.hud->is_destroyed(),
              "same-key room reuse keeps one live minimap");
        std::vector<elysia::core::UiRenderCommand> hud_commands;
        observation.hud->submit_ui_render_commands(hud_commands);
        check(!hud_commands.empty() && hud_commands.back().type == elysia::core::UiRenderCommandType::FillCircle,
              "destination minimap already shows its red marker before the first update");

        update(0);
        check(flow.get_current_room_id() == to && flow.find_room(to)->visited && flow.find_room(to)->discovered,
              "successful boundary entry commits visit state");
        const auto before = player().center();
        if (diagonal) keys({direction_key(direction), normal.x == 0 ? Key::KeyD : Key::KeyS});
        else keys({direction_key(direction)}); // No new press event: the key is still held.
        update(1.0 / 60.0);
        const auto moved = player().center() - before;
        check(moved.x * normal.x + moved.y * normal.y > 1, "held movement continues without another key press");
        check(observation.builds == expected_builds, "inward movement never bounces to the source");
        keys({});
        update(1.0 / 60.0);
        check(player().velocity().is_zero(), "releasing a transferred key stops movement");
    }
};

void start_gate_delay()
{
    BoundaryTestRun run;
    const auto gates_raised = [&] {
        const auto gates = ELYSIA_OBJECT_QUERY->find_objects<Gate>();
        check(gates.size() == 20, "Start has four rows of five gates");
        return std::ranges::all_of(gates, &Gate::is_raised);
    };
    check(gates_raised(), "Start gates are raised on the first frame");
    for (const auto* gate : ELYSIA_OBJECT_QUERY->find_objects<Gate>())
        check(gate->render_rect().height() == 48, "Start gates begin visually raised without an opening animation");

    const auto strip = corridor(run.map(), RoomDirection::Right).gate_strip(run.map().tile_size());
    const Vector2 start_center{strip.left() - 16, strip.center().y};
    check(run.player().physics_world()->teleport_object(run.player().physics_handle(),
        start_center - run.player().size() * 0.5f), "place the player inside the Start gate row");
    run.update(0);
    run.keys({Key::KeyD});
    run.update(0.25);
    check(gates_raised(), "Start gates remain raised during the opening delay");
    check(run.player().blocking_bounds().right() <= strip.left() + 1.0f,
          "Start gates physically block the player during the opening delay");

    run.observation.scene->pause();
    run.update(2);
    check(gates_raised(), "paused time does not consume the opening delay");
    run.observation.scene->resume();
    run.keys({});
    run.update(0.5);
    run.update(0.125);
    check(gates_raised(), "gates remain raised before one second of active game time");
    run.update(0.125);
    check(std::ranges::none_of(ELYSIA_OBJECT_QUERY->find_objects<Gate>(), &Gate::is_raised),
          "all gates lower after exactly one active second");
    for (const auto* gate : ELYSIA_OBJECT_QUERY->find_objects<Gate>())
    {
        std::vector<elysia::core::RenderCommand> commands;
        gate->submit_render_commands(commands);
        check(commands.size() == 1 && commands.back().src_rect == elysia::core::Rect{128, 0, 32, 48},
              "Start gates are halfway through lowering at this animation frame");
    }
    run.keys({Key::KeyD});
    for (int frame = 0; frame < 24; ++frame)
        run.update(1.0 / 60.0);
    for (const auto* gate : ELYSIA_OBJECT_QUERY->find_objects<Gate>())
        check(gate->render_rect() == gate->world_rect(), "Start's lowering animation reaches its resting endpoint");
    check(run.player().blocking_bounds().left() > strip.right(), "the lowered Start gate row allows passage");
    run.keys({});

    run.cross(RoomDirection::Right);
    run.cross(RoomDirection::Left);
    check(std::ranges::none_of(ELYSIA_OBJECT_QUERY->find_objects<Gate>(), &Gate::is_raised),
          "returning to Start does not repeat the opening delay");

    auto& flow = *GameFlowManager::instance();
    const auto character = run.player().capture_context();
    flow.restart(43);
    run.manager.on_scene_request({elysia::scene::SceneRequestType::Switch, make_run_room_route(0, character)});
    run.update(0);
    check(gates_raised(), "a new run rearms the delay even when reusing the Start scene");
    run.observation.room_locked = true;
    run.update(1);
    check(gates_raised(), "room restrictions still hold the gates after the delay expires");
    run.observation.room_locked = false;
    run.update(0);
    check(std::ranges::none_of(ELYSIA_OBJECT_QUERY->find_objects<Gate>(), &Gate::is_raised),
          "normal room control resumes once the opening delay ends");
}

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

void corridor_layouts()
{
    for (int gap = 1; gap <= 4; ++gap)
        for (const auto direction : {RoomDirection::Up, RoomDirection::Right, RoomDirection::Down, RoomDirection::Left})
        {
            Floor floor;
            const auto normal = game::corridor_normal(direction);
            const FloorPosition from{normal.x < 0 ? 5 : normal.x > 0 ? 0 : 2,
                                     normal.y < 0 ? 5 : normal.y > 0 ? 0 : 2};
            const FloorPosition to{from.x + static_cast<int>(normal.x) * (gap + 1),
                                   from.y + static_cast<int>(normal.y) * (gap + 1)};
            floor.rooms[from.y][from.x] = Room{.id = 0, .type = RoomType::Start};
            floor.rooms[to.y][to.x] = Room{.id = 1, .type = RoomType::End};
            check(floor.connect(from, to), "connect a cardinal gap fixture");
            check(game::connection_gap(floor, 0, 1) == gap, "count only empty cells between endpoints");
            const auto layout = game::make_corridor_layout(floor, {0, 1, direction});
            bool wrong_direction_rejected = false;
            try { (void)game::make_corridor_layout(floor, {0, 1, game::opposite_direction(direction)}); }
            catch (const std::invalid_argument&) { wrong_direction_rejected = true; }
            check(wrong_direction_rejected, "layout rejects a direction pointing away from its target");
            const float expected = static_cast<float>(gap == 1 ? 13 : gap == 2 ? 19 : 25);
            check(layout.source_seam.distance_to(layout.target_seam) == expected,
                  "added traversal distance is 13/19/25 tiles and caps at three empty cells");
            MapConfig config;
            config.baseRoomSize = layout.grid_size;
            config.numSubRooms = 0;
            DungeonRoom map(config, layout.grid_size);
            map.set_position({96, -160});
            const auto exits = layout.world_exits(map.world_origin(), map.tile_size());
            check(exits.size() == 2 && exits.front().direction == game::opposite_direction(direction)
                && exits.back().direction == direction, "corridor exits lead to its original source and target");
            const auto source = exits.front().seam_center();
            const auto target = exits.back().seam_center();
            check(test::walkable(map, Rect::from_center(source, {32, 32}))
                && test::walkable(map, Rect::from_center(target, {32, 32})),
                  "both transfer seams have walkable buffer space");
            check(layout.progress(source, map.world_origin(), map.tile_size()) == 0
                && layout.progress(target, map.world_origin(), map.tile_size()) == 1
                && std::abs(layout.progress((source + target) * 0.5f, map.world_origin(), map.tile_size()) - 0.5f) < 0.001f,
                  "corridor progress handles all four directions and nonzero world origins");
        }
    check(game::extra_corridor_tiles(0) == 0, "adjacent rooms have no extra corridor length");
}

RoomDirection travel_to_gap(BoundaryTestRun& run, RoomID parent = std::numeric_limits<RoomID>::max())
{
    const auto& flow = *GameFlowManager::instance();
    const RoomID current = flow.get_current_room_id();
    for (const auto direction : {RoomDirection::Up, RoomDirection::Right, RoomDirection::Down, RoomDirection::Left})
    {
        const auto* next = flow.connected_room(current, direction);
        if (next && next->id != parent && game::connection_gap(flow.current_floor(), current, next->id) > 0)
            return direction;
    }
    for (const auto direction : {RoomDirection::Up, RoomDirection::Right, RoomDirection::Down, RoomDirection::Left})
    {
        const auto* next = flow.connected_room(current, direction);
        if (!next || next->id == parent) continue;
        run.cross(direction);
        const auto found = travel_to_gap(run, current);
        if (found != RoomDirection::None) return found;
        run.cross(game::opposite_direction(direction));
    }
    return RoomDirection::None;
}

void corridor_return_and_validation()
{
    BoundaryTestRun run;
    auto& flow = *GameFlowManager::instance();
    const auto direction = travel_to_gap(run);
    check(direction != RoomDirection::None, "generated test floor contains a gap connection");
    const CorridorTraversal traversal{flow.get_current_room_id(), flow.connected_room(flow.get_current_room_id(), direction)->id,
                                       direction};
    const Room before = *flow.find_room(traversal.to);
    auto route = make_boundary_room_route(traversal.from, direction, run.player().capture_context(), {});
    for (int scenario = 0; scenario < 6; ++scenario)
    {
        auto payload = *elysia::scene::try_scene_payload<RoomScenePayload>(route.payload);
        switch (scenario)
        {
        case 0: payload.corridor->to = traversal.from; break;
        case 1: payload.corridor->direction = game::opposite_direction(direction); break;
        case 2: payload.room.grid_size.x += 1; break;
        case 3: payload.room_id = traversal.from; break;
        case 4: payload.boundary_entry.reset(); break;
        case 5: payload.corridor->from = traversal.to; break;
        }
        bool rejected = false;
        try { validate_room_entry(payload); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "malformed corridor endpoints, dimensions and handoffs are rejected");
    }

    for (const float fraction : {0.0f, 0.5f, 0.9f})
    {
        route = make_boundary_room_route(traversal.from, direction, run.player().capture_context(), {});
        run.manager.on_scene_request({elysia::scene::SceneRequestType::Switch, route});
        run.update(0);
        check(run.manager.current_scene_key() == Corridor, "gap route enters Corridor");
        const auto layout = game::make_corridor_layout(flow.current_floor(), traversal);
        const auto exits = layout.world_exits(run.map().world_origin(), run.map().tile_size());
        const auto center = exits.front().seam_center()
            + (exits.back().seam_center() - exits.front().seam_center()) * fraction;
        check(run.player().physics_world()->teleport_object(run.player().physics_handle(), center - run.player().size() * 0.5f),
              "position the player at a corridor turn-back point");
        run.update(0);
        const int builds = run.observation.builds;
        for (int frame = 0; frame < 600 && run.observation.builds == builds; ++frame)
        {
            run.keys({direction_key(game::opposite_direction(direction)), Key::KeyF});
            run.update(1.0 / 60.0);
            check(ELYSIA_OBJECT_QUERY->find_objects<Bullet>().empty(), "holding Fire in a corridor never spawns bullets");
        }
        check(run.manager.current_scene_key() == FixedRoom && run.observation.builds == builds + 1,
              "turning back at the entrance, middle or far end returns to the source room once");
        check(near(hud_rooms(*run.observation.hud).front(), run.observation.hud_positions.front(), 1.0f),
              "returning through the corridor entrance preserves the HUD projection");
        run.keys({});
        run.update(0);
        check(flow.get_current_room_id() == traversal.from
            && flow.find_room(traversal.to)->visited == before.visited
            && flow.find_room(traversal.to)->discovered == before.discovered,
              "returning to the current source room does not mark or reveal the target");
    }

    // An unbuildable destination must not commit the target, even after a successful corridor entry.
    route = make_boundary_room_route(traversal.from, direction, run.player().capture_context(), {});
    run.manager.on_scene_request({elysia::scene::SceneRequestType::Switch, route});
    run.update(0);
    auto failed = make_corridor_room_route(traversal, direction, run.player().capture_context(), {});
    auto payload = *elysia::scene::try_scene_payload<RoomScenePayload>(failed.payload);
    payload.room.grid_size = {2, 2};
    payload.room.map_config.baseRoomSize = {2, 2};
    payload.room.map_config.corridors.directions = RoomDirection::None;
    failed.payload = payload;
    run.manager.on_scene_request({elysia::scene::SceneRequestType::Switch, failed});
    run.manager.on_update(0);
    check(flow.get_current_room_id() == traversal.from && flow.find_room(traversal.to)->visited == before.visited,
          "failed destination construction leaves corridor progression unchanged");
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
    check(crossed_gap && flow.get_current_room_id() == 0, "gap corridors and direct links return to Start");

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
    corridor_layouts();
    start_gate_delay();
    boundary_transitions();
    corridor_return_and_validation();
    exact_midpoint_and_reverse();
    invalid_entries();
}
