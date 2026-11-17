#include "scene_harness.h"
#include "game/objcets/portal.h"
#include "game/map/dungeon_room.h"
#include "game/map/room_placement.h"
#include "game/characters/character_manager.h"
#include "game/characters/player_character.h"
#include "game/characters/enemy.h"
#include "game/application/game_module.h"
#include "game/gameflow/game_flow_manager.h"
#include "game/scene/run/combat_scene.h"
#include "game/scene/scene_keys.h"
#include "game/ui/floor_map_view.h"
#include "engine/core/render/render_command.h"
#include "engine/object_query/game_object_query_service.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>

namespace
{
using elysia::core::Rect;
using elysia::core::Vector2;

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

template<class Function> void rejects(Function&& function, const char* message)
{
    bool rejected = false;
    try { function(); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected, message);
}

void portal_trigger()
{
    Portal portal({100, 100, 32, 32});
    int reached = 0;
    portal.set_on_reached([&] {
        check(portal.is_reached(), "mark reached before callback");
        ++reached;
        portal.check_reached(portal.world_rect()); // Reentry cannot invoke twice.
    });
    check(portal.depth_layer() == elysia::core::DepthLayer::Item, "portal above terrain");
    for (Rect outside : {Rect{0, 0, 32, 32}, Rect{68, 100, 32, 32},
                         Rect{132, 100, 32, 32}, Rect{100, 68, 32, 32},
                         Rect{100, 132, 32, 32}, Rect{110, 110, 0, 10}})
        portal.check_reached(outside);
    check(reached == 0 && !portal.is_reached(), "outside, edges and zero area do not trigger");
    portal.set_active(false);
    portal.check_reached(portal.world_rect());
    check(!portal.is_reached(), "inactive portal cannot trigger");
    portal.set_active(true);
    portal.destroy();
    portal.check_reached(portal.world_rect());
    check(!portal.is_reached(), "destroyed portal cannot trigger");
    portal.reset();
    portal.check_reached({131, 110, 10, 10});
    portal.check_reached(portal.world_rect());
    portal.check_reached({0, 0, 1, 1});
    portal.check_reached(portal.world_rect());
    check(reached == 1, "overlap triggers once, including after leaving and returning");
    portal.reset();
    portal.check_reached(portal.world_rect());
    check(reached == 2, "reset preserves callback and rearms portal");
    Portal empty_callback({0, 0, 32, 32});
    empty_callback.check_reached(empty_callback.world_rect());
    check(empty_callback.is_reached(), "no callback still records reach");
    std::vector<elysia::core::RenderCommand> commands;
    portal.submit_render_commands(commands);
    check(commands.size() == 2 && commands.front().command_rect == portal.world_rect(),
          "portal renders fill and border");
}

void placement()
{
    MapConfig config;
    config.numSubRooms = 0;
    DungeonRoom map(config, {12, 12});
    map.set_position({128, -64});
    const auto portal = game::central_portal_bounds(map);
    check(portal.center() == map.center() && portal.size() == game::kPortalSize,
          "central portal respects map origin");
    check(game::walkable_bounds(map, portal), "central portal on floor");
    const auto spawn = game::player_spawn_near_portal(map);
    check(spawn == map.position() + Vector2{160, 128}, "equal distances resolve by row then column");
    const Rect player{spawn, game::kPlayerSpawnSize};
    check(game::walkable_bounds(map, player) && !portal.intersects(player), "separate full player bounds");
    check(!game::walkable_bounds(map, {map.position(), {32, 32}}), "void is not floor");
    check(!game::walkable_bounds(map, {map.position() - Vector2{1, 0}, {32, 32}}), "outside map rejected");
    check(!game::walkable_bounds(map, {map.position() + Vector2{48, 48}, {32, 32}}),
          "full rectangle crossing wall rejected");
    rejects([&] { (void)game::find_room_spawn(map, {10000, 10000}, map.center(), {}); },
            "no complete walkable spawn rejected");
    const Rect occupied = map.world_rect();
    rejects([&] { (void)game::find_room_spawn(map, {32, 32}, map.center(), {&occupied, 1}); },
            "fully occupied room rejected");
    config.baseRoomSize = {2, 2};
    DungeonRoom blocked(config, {12, 12});
    rejects([&] { (void)game::player_spawn_near_portal(blocked); }, "blocked center rejected");
}

std::vector<TileType> layout(const DungeonRoom& map)
{
    std::vector<TileType> result;
    for (const auto& row : map.tile_map().data())
        for (const auto& tile : row)
            result.push_back(tile.type);
    return result;
}

void click(elysia::scene::SceneManager& manager, int x, int y)
{
    using namespace elysia::input;
    RawInputState mouse;
    for (const auto type : {RawInputEventType::ControlPressed, RawInputEventType::ControlReleased})
    {
        const RawInputEvent event{
            .control = RawInputControl::MouseLeft,
            .type = type,
            .device = InputDevice::Mouse,
            .mouse_x = x,
            .mouse_y = y,
            .source = InputSourceId::mouse(),
        };
        InputSnapshot snapshot;
        snapshot.sources = {{.source = event.source, .initial_state = mouse}};
        apply_raw_event(mouse, event);
        snapshot.sources.front().frame.state = mouse;
        snapshot.events = {event};
        manager.on_input(snapshot);
    }
}

void fixed_floor_topology()
{
    const GameFlowManager flow;
    const auto& floor = flow.current_floor();
    check(floor.rooms.size() == 6 && floor.rooms.front().size() == 6, "floor uses a 6 by 6 grid");
    check(floor.start_room_id == 0 && floor.end_room_id == 13, "start and exit retain their IDs");
    check(floor.find_position(0) == FloorPosition{2, 2}, "start is at the translated grid position");
    check(!flow.find_room(14) && !flow.find_room(std::numeric_limits<RoomID>::max()),
          "unknown IDs do not resolve to a room");
    check(!floor.room_at({-1, 2}) && !floor.room_at({6, 2}) && !floor.room_at({0, 0}),
          "room lookup handles the edges and empty cells");

    int room_count = 0;
    int connection_count = 0;
    for (int y = 0; y < Floor::height; ++y)
    {
        for (int x = 0; x < Floor::width; ++x)
        {
            const FloorPosition position{x, y};
            if (!floor.room_at(position))
                continue;

            ++room_count;
            for (const auto& neighbor : floor.neighbors(position))
            {
                if (!neighbor)
                    continue;

                ++connection_count;
                const auto reverse = floor.neighbors(*neighbor);
                check(std::ranges::find(reverse, std::optional{position}) != reverse.end(),
                      "each connection is bidirectional");
            }
        }
    }
    check(room_count == 14 && connection_count == 34, "fixed floor has 14 rooms and 17 connections");
    check(floor.room_at({2, 2})->accessible_directions == RoomDirection::All, "start connects in all directions");

    const auto east_neighbors = floor.neighbors(*floor.find_position(2));
    check(std::ranges::find(east_neighbors, floor.find_position(10)) == east_neighbors.end(),
          "adjacent rooms 2 and 10 are not connected");

    auto one_way = floor;
    one_way.room_at({2, 1})->accessible_directions = RoomDirection::None;
    check(!one_way.neighbors({2, 2}).front(), "a direction bit without its reverse cannot be traversed");

    std::array<int, 14> distance;
    distance.fill(-1);
    distance[floor.start_room_id] = 0;
    std::queue<RoomID> pending;
    pending.push(floor.start_room_id);
    while (!pending.empty())
    {
        const auto id = pending.front();
        pending.pop();
        for (const auto& neighbor : floor.neighbors(*floor.find_position(id)))
        {
            if (!neighbor)
                continue;

            const auto next = floor.room_at(*neighbor)->id;
            check(next < distance.size(), "the fixed floor retains its original ID range");
            if (distance[next] < 0)
            {
                distance[next] = distance[id] + 1;
                pending.push(next);
            }
        }
    }
    check(std::ranges::count(distance, -1) == 0, "every room is reachable");
    check(distance[floor.end_room_id] == 4, "the shortest start-to-exit route takes four moves");
}

void floor_map_directions()
{
    using elysia::core::UiRenderCommand;
    using elysia::core::UiRenderCommandType;

    GameFlowManager flow;
    const auto draw_map = [&] {
        FloorMapView view(Rect{0, 0, 1280, 720}, flow.current_floor(),
                          [&](RoomID id) { return flow.can_enter_room(id); },
                          [](RoomID) {});
        std::vector<UiRenderCommand> commands;
        view.submit_ui_render_commands(commands);
        return commands;
    };
    const auto line_count = [](const auto& commands, elysia::core::Color color) {
        return std::ranges::count_if(commands, [color](const auto& command) {
            return command.type == UiRenderCommandType::DrawLine && command.color == color;
        });
    };

    const auto start = draw_map();
    check(line_count(start, elysia::core::colors::gray_500) == 4
        && line_count(start, elysia::core::colors::gray_300) == 16,
        "the starting map shows arrows in both directions on each revealed connection");

    check(flow.try_enter_room(1), "the first room can be entered");
    const auto explored = draw_map();
    check(line_count(explored, elysia::core::colors::gray_500) == 6
        && line_count(explored, elysia::core::colors::gray_300) == 24,
        "exploration reveals new direction markers with their connections");
}

void single_floor_run()
{
    using elysia::scene::SceneManager;
    auto& flow = *GameFlowManager::instance();
    const auto initial_character = *CharacterManager::instance()->create_context("default_player");
    elysia::io::ContentRegistry content;
    elysia::scene::SceneRuntimeContext context{nullptr, content, 1280, 720};
    SceneManager manager;
    manager.initialize(context);
    GameModule{}.register_scenes(manager);
    flow.restart(42);
    manager.start(make_run_room_route(0, initial_character));
    check(manager.current_scene_key() == FixedRoom && flow.get_current_room_id() == 0,
          "run opens the fixed starting room");
    for (RoomID id = 0; id < 14; ++id)
    {
        const auto* room = flow.find_room(id);
        check(room && room->discovered == (id <= 4 || id == 13) && room->visited == (id == 0),
              "initial fog shows the start, neighbors and exit");
    }

    const auto* query = ELYSIA_OBJECT_QUERY;
    const auto* start_map = query->find_object<DungeonRoom>();
    check(start_map && start_map->columns() == 15 && start_map->rows() == 15, "fixed start room dimensions");
    check(query->find_objects<Enemy>().empty(), "start room is safe");
    const auto enter_portal = [&] {
        auto* player = query->find_object<PlayerCharacter>();
        const auto* portal = query->find_object<Portal>();
        check(player && portal, "active room has a player and portal");
        player->set_position(portal->position());
        manager.on_update(0);
    };
    enter_portal();
    check(manager.current_scene_key() == FloorMap, "start portal opens the map");
    click(manager, 1025, 307);
    click(manager, 717, 307);
    check(manager.current_scene_key() == FloorMap && flow.get_current_room_id() == 0,
          "disabled exit and hidden nodes do not request a route");

    click(manager, 563, 307);
    check(manager.current_scene_key() == CombatRoom && flow.get_current_room_id() == 0,
          "selection loads the battle room before committing movement");
    manager.on_update(0);
    check(flow.get_current_room_id() == 1 && flow.find_room(5)->discovered && !flow.find_room(5)->visited,
          "active room commits the move and reveals its neighbors");
    const auto* first_map = query->find_object<DungeonRoom>();
    check(first_map != nullptr, "combat room has a map");
    const auto first_layout = layout(*first_map);
    const auto enemies = query->find_objects<Enemy>();
    check(!enemies.empty(), "first visit spawns enemies");
    enter_portal();
    check(manager.current_scene_key() == CombatRoom && !query->find_object<Portal>()->is_reached(),
          "living enemies block the portal without consuming it");
    auto* player = query->find_object<PlayerCharacter>();
    player->receive_attack({0, 0, 10});
    const float health = player->health();
    check(health < initial_character.health && health > 0, "test damaged the player");
    for (auto* enemy : enemies)
        enemy->receive_attack({0, 0, enemy->max_health()});
    manager.on_update(0);
    check(manager.current_scene_key() == FloorMap && flow.find_room(1)->cleared,
          "defeated enemies enable the portal and mark the room cleared");
    click(manager, 563, 413);
    manager.on_update(0);
    check(manager.current_scene_key() == FixedRoom && flow.get_current_room_id() == 0,
          "visited start can be reentered");
    check(query->find_object<PlayerCharacter>()->health() == health, "health survives scene transitions");
    enter_portal();
    click(manager, 563, 307);
    manager.on_update(0);
    check(manager.current_scene_key() == CombatRoom && flow.get_current_room_id() == 1,
          "cleared combat room remains selectable");
    check(query->find_objects<Enemy>().empty(), "cleared combat does not respawn enemies");
    check(layout(*query->find_object<DungeonRoom>()) == first_layout, "revisit retains seeded terrain");

    enter_portal();
    click(manager, 717, 307);
    manager.on_update(0);
    check(manager.current_scene_key() == FixedRoom && flow.get_current_room_id() == 5, "event uses a fixed room");
    enter_portal();
    click(manager, 1025, 307); // The visible exit is beyond unvisited rooms.
    click(manager, 717, 307); // The current room.
    check(manager.current_scene_key() == FloorMap && flow.get_current_room_id() == 5,
          "routes through unvisited rooms and the current room remain disabled");

    check(flow.find_room(9)->discovered && !flow.find_room(9)->visited, "room 9 is visible but unvisited");
    click(manager, 563, 201); // Room 9 is reachable through visited room 1.
    manager.on_update(0);
    check(manager.current_scene_key() == FixedRoom && flow.get_current_room_id() == 9
        && flow.find_room(9)->visited && query->find_object<PlayerCharacter>()->health() == health,
        "reachable unexplored room can be selected through a visited room");
    enter_portal();

    click(manager, 563, 413);
    manager.on_update(0);
    check(manager.current_scene_key() == FixedRoom && flow.get_current_room_id() == 0
        && query->find_object<PlayerCharacter>()->health() == health,
        "visited non-neighbor can be selected directly with player state intact");
    enter_portal();
    click(manager, 717, 307);
    manager.on_update(0);
    check(manager.current_scene_key() == FixedRoom && flow.get_current_room_id() == 5,
          "fast travel works in both directions between visited rooms");
    enter_portal();
    click(manager, 871, 307);
    manager.on_update(0);
    check(manager.current_scene_key() == CombatRoom && flow.get_current_room_id() == 6, "elite routes to combat");
    for (auto* enemy : query->find_objects<Enemy>())
        enemy->receive_attack({0, 0, enemy->max_health()});
    enter_portal();
    click(manager, 1025, 307);
    manager.on_update(0);
    check(manager.current_scene_key() == FixedRoom && flow.get_current_room_id() == 13, "route reaches the exit");
    enter_portal();
    check(manager.current_scene_key() == MainMenu, "exit portal returns directly to the main menu");

    click(manager, 1155, 38); // Top-right test entry.
    check(manager.current_scene_key() == Game && flow.get_current_room_id() == 13,
          "menu test launches GameScene outside the run");
    manager.on_scene_request({elysia::scene::SceneRequestType::Switch, {.target = MainMenu}});
    manager.on_update(0);
    click(manager, 640, 281); // Start below the logo.
    check(manager.current_scene_key() == FixedRoom && flow.get_current_room_id() == 0
        && !flow.find_room(1)->cleared && !flow.find_room(5)->discovered,
        "menu Start creates a fresh run");

    flow.restart(43);
    check(flow.get_current_room_id() == 0 && flow.room_seed(1) == 44 && !flow.find_room(1)->cleared
        && !flow.find_room(5)->discovered, "restart resets clears, fog and seed");

    auto failed_route = make_run_room_route(1, initial_character, 0);
    auto room = *elysia::scene::try_scene_payload<RoomScenePayload>(failed_route.payload);
    room.room.map_config.baseRoomSize = {2, 2};
    room.room.map_config.numSubRooms = 0;
    failed_route.payload = room;
    manager.on_scene_request({elysia::scene::SceneRequestType::Switch, failed_route});
    manager.on_update(0);
    check(manager.current_scene_key() != CombatRoom && flow.get_current_room_id() == 0
        && !flow.find_room(1)->visited && !flow.find_room(5)->discovered, "failed room load leaves exploration untouched");
}

template<class T> std::vector<T*> objects(elysia::scene::Scene& scene)
{
    std::vector<T*> result;
    static_cast<const elysia::object_query::IGameObjectQueryRuntime&>(scene)
        .visit_game_objects(elysia::core::DepthLayerMask::all(), [&](auto& object) {
            if (!object.is_destroyed())
                if (auto* value = dynamic_cast<T*>(&object)) result.push_back(value);
            return true;
        });
    return result;
}

template<class T> void room_portal(const elysia::scene::ScenePayload& payload = {})
{
    test::SceneHarness<T> host(payload);
    auto& scene = host.scene();
    for (int visit = 0; visit < 3; ++visit)
    {
        if (visit)
            host.enter(payload, visit == 1 ? elysia::scene::SceneReloadMode::Reuse
                                          : elysia::scene::SceneReloadMode::Reset);
        auto portals = objects<Portal>(scene);
        auto players = objects<PlayerCharacter>(scene);
        auto maps = objects<DungeonRoom>(scene);
        check(portals.size() == 1 && players.size() == 1 && maps.size() == 1, "one portal per room visit");
        auto* portal = portals.front();
        auto* player = players.front();
        auto* map = maps.front();
        const auto initial_player = player->world_rect();
        check(portal->center() == map->center() && portal->size() == game::kPortalSize, "portal at map center");
        check(game::walkable_bounds(*map, initial_player)
            && !portal->world_rect().intersects(initial_player), "safe player spawn");
        for (auto* enemy : objects<Enemy>(scene))
            check(game::walkable_bounds(*map, enemy->world_rect())
                && !enemy->world_rect().intersects(initial_player)
                && !enemy->world_rect().intersects(portal->world_rect()), "enemies avoid player and portal");
        int reached = 0;
        portal->set_on_reached([&] { ++reached; });
        host.update(0);
        check(reached == 0, "spawn cannot trigger portal");
        player->set_position(portal->position());
        scene.pause();
        host.update(0);
        check(reached == 0, "paused room does not trigger");
        scene.resume();
        player->set_active(false);
        host.update(0);
        check(reached == 0, "inactive player does not trigger");
        player->set_active(true);
        portal->set_active(false);
        host.update(0);
        check(reached == 0, "inactive portal does not trigger in room");
        portal->set_active(true);
        host.update(0);
        host.update(0);
        check(reached == 1 && portal->is_reached(), "room dispatches once");
        portal->reset();
        player->receive_attack({0, 0, player->max_health()});
        host.update(0);
        check(reached == 1 && !portal->is_reached(), "dead player does not trigger");
        if (visit == 1)
        {
            player->destroy();
            host.update(0);
            host.update(0);
            check(objects<PlayerCharacter>(scene).empty(), "player removal is safe");
        }
        if (visit != 0)
        {
            portal->destroy();
            host.update(0);
            host.update(0);
            check(objects<Portal>(scene).empty() && reached == 1, "portal removal clears observer");
        }
        host.leave();
        check(objects<Portal>(scene).empty() && objects<PlayerCharacter>(scene).empty() && reached == 1,
              "exit clears room objects without invoking callback");
    }
}
}

int main()
{
    try
    {
        portal_trigger();
        placement();
        room_portal<CombatScene>(RoomScenePayload{
            *CharacterManager::instance()->create_context("default_player"), {}});
        fixed_floor_topology();
        floor_map_directions();
        single_floor_run();
        std::cout << "Portal tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
