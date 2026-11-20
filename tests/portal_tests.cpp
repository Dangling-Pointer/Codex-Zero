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
#include "engine/ui/widgets/ui_button.h"

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

constexpr RoomID start_room_id = 0;
constexpr RoomID end_room_id = 13;

struct TestRoomDefinition
{
    RoomID id;
    RoomType type;
    FloorPosition position;
};

Floor make_fixed_floor()
{
    // Grid x increases to the right, and y increases downwards.
    constexpr TestRoomDefinition rooms[] = {
        {start_room_id, RoomType::Start, {2, 2}},
        {1, RoomType::Combat, {2, 1}},
        {2, RoomType::Combat, {3, 2}},
        {3, RoomType::Rest, {2, 3}},
        {4, RoomType::Combat, {1, 2}},
        {5, RoomType::Event, {3, 1}},
        {6, RoomType::Elite, {4, 1}},
        {7, RoomType::Combat, {4, 2}},
        {8, RoomType::Combat, {5, 2}},
        {9, RoomType::Event, {2, 0}},
        {10, RoomType::Shop, {3, 3}},
        {11, RoomType::Rest, {0, 2}},
        {12, RoomType::Event, {1, 3}},
        {end_room_id, RoomType::End, {5, 1}},

        // Fill the remaining cells to exercise the full map layout.
        {14, RoomType::Rest, {0, 0}},
        {15, RoomType::Combat, {1, 0}},
        {16, RoomType::Shop, {3, 0}},
        {17, RoomType::Combat, {4, 0}},
        {18, RoomType::Event, {5, 0}},
        {19, RoomType::Combat, {0, 1}},
        {20, RoomType::Elite, {1, 1}},
        {21, RoomType::Event, {0, 3}},
        {22, RoomType::Elite, {4, 3}},
        {23, RoomType::Rest, {5, 3}},
        {24, RoomType::Combat, {0, 4}},
        {25, RoomType::Rest, {1, 4}},
        {26, RoomType::Combat, {2, 4}},
        {27, RoomType::Shop, {3, 4}},
        {28, RoomType::Combat, {4, 4}},
        {29, RoomType::Event, {5, 4}},
        {30, RoomType::Shop, {0, 5}},
        {31, RoomType::Combat, {1, 5}},
        {32, RoomType::Event, {2, 5}},
        {33, RoomType::Elite, {3, 5}},
        {34, RoomType::Rest, {4, 5}},
        {35, RoomType::Combat, {5, 5}},
    };

    constexpr std::pair<RoomID, RoomID> connections[] = {
        {0, 1}, {1, 5}, {5, 6}, {6, 13}, // Upper route
        {0, 2}, {2, 7}, {7, 8}, {8, 13}, // Right route
        {2, 5}, {6, 7},                  // Links between routes
        {0, 3}, {3, 12}, {12, 4}, {4, 0}, // Southwest loop
        {1, 9}, {3, 10}, {4, 11},         // Side branches

        {9, 15}, {15, 14}, {14, 19}, {19, 20},
        {9, 16}, {16, 17}, {17, 18},
        {11, 21}, {21, 24}, {24, 30}, {30, 31}, {31, 25},
        {25, 26}, {26, 32}, {32, 33}, {33, 27},
        {10, 22}, {22, 23}, {23, 29}, {29, 35}, {35, 34}, {34, 28},
    };

    Floor floor{
        .start_room_id = start_room_id,
        .end_room_id = end_room_id,
        .current_room_id = start_room_id,
    };
    for (const auto& room : rooms)
        floor.rooms[room.position.y][room.position.x].emplace(Room{.id = room.id, .type = room.type});

    for (const auto& [first, second] : connections)
    {
        const auto from = floor.find_position(first);
        const auto to = floor.find_position(second);
        if (!from || !to || !floor.connect(*from, *to))
            throw std::logic_error("Invalid connection in the fixed floor");
    }
    return floor;
}

void fixed_floor_topology()
{
    const auto floor = make_fixed_floor();
    check(floor.rooms.size() == 6 && floor.rooms.front().size() == 6, "floor uses a 6 by 6 grid");
    check(floor.start_room_id == 0 && floor.end_room_id == 13, "start and exit retain their IDs");
    check(floor.find_position(0) == FloorPosition{2, 2}, "start is at the translated grid position");
    check(!floor.find_position(36) && !floor.find_position(std::numeric_limits<RoomID>::max()),
          "unknown IDs do not resolve to a room");
    check(!floor.room_at({-1, 2}) && !floor.room_at({6, 2}) && !Floor{}.room_at({0, 0}),
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
    check(room_count == 36 && connection_count == 78, "test floor fills all 36 cells with 39 connections");
    check(floor.room_at({2, 2})->accessible_directions == RoomDirection::All, "start connects in all directions");

    const auto east_neighbors = floor.neighbors(*floor.find_position(2));
    check(std::ranges::find(east_neighbors, floor.find_position(10)) == east_neighbors.end(),
          "adjacent rooms 2 and 10 are not connected");

    auto one_way = floor;
    one_way.room_at({2, 1})->accessible_directions = RoomDirection::None;
    check(!one_way.neighbors({2, 2}).front(), "a direction bit without its reverse cannot be traversed");

    std::array<int, 36> distance;
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

    auto floor = make_fixed_floor();
    floor.room_at(*floor.find_position(0))->visited = true;
    for (RoomID id : {0, 1, 2, 3, 4, 13})
        floor.room_at(*floor.find_position(id))->discovered = true;

    const auto can_enter_room = [](RoomID) { return false; };
    const auto draw_map = [&] {
        FloorMapView view(Rect{0, 0, 1280, 720}, floor, can_enter_room, [](RoomID) {});
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

    floor.current_room_id = 1;
    floor.room_at(*floor.find_position(1))->visited = true;
    for (const auto& neighbor : floor.neighbors(*floor.find_position(1)))
    {
        if (neighbor)
            floor.room_at(*neighbor)->discovered = true;
    }
    const auto explored = draw_map();
    check(line_count(explored, elysia::core::colors::gray_500) == 6
        && line_count(explored, elysia::core::colors::gray_300) == 24,
        "exploration reveals new direction markers with their connections");

    const Rect viewport{0, 0, 1280, 720};
    FloorMapView preview(viewport, floor, can_enter_room,
                         [](RoomID) {}, FloorMapView::Visibility::ShowAll);
    std::vector<UiRenderCommand> preview_commands;
    preview.submit_ui_render_commands(preview_commands);
    check(line_count(preview_commands, elysia::core::colors::gray_500) == 39
        && line_count(preview_commands, elysia::core::colors::gray_300) == 156,
        "full-map preview draws every connection and its directions");

    std::vector<Rect> button_bounds;
    for (std::size_t i = 0; i < preview.child_count(); ++i)
    {
        const auto* button = dynamic_cast<const elysia::ui::UiButton*>(preview.child_at(i));
        if (!button)
            continue;

        const auto bounds = button->screen_rect();
        check(viewport.contains(bounds), "full-map room buttons fit inside the viewport");
        check(std::ranges::none_of(button_bounds, [&](const Rect& other) { return other.intersects(bounds); }),
              "full-map room buttons do not overlap");
        button_bounds.push_back(bounds);
    }
    check(button_bounds.size() == 36, "all 36 room buttons are visible in the preview");
}

void sparse_floor_connections()
{
    Floor floor;
    floor.rooms[0][0] = Room{.id = 0, .type = RoomType::Start, .discovered = true, .visited = true};
    floor.rooms[0][3] = Room{.id = 1, .type = RoomType::Event, .discovered = true};
    floor.rooms[4][3] = Room{.id = 2, .type = RoomType::End};
    floor.start_room_id = 0;
    floor.end_room_id = 2;
    check(floor.connect({0, 0}, {3, 0}) && floor.connect({3, 0}, {3, 4}),
          "aligned rooms connect across empty cells");
    check(floor.neighbors({0, 0})[1] == FloorPosition{3, 0}
        && floor.neighbors({3, 0})[3] == FloorPosition{0, 0}
        && floor.neighbors({3, 0})[2] == FloorPosition{3, 4}
        && floor.neighbors({3, 4})[0] == FloorPosition{3, 0},
        "long connections are traversable in both directions");
    check(!floor.neighbors({0, 0})[0] && !floor.neighbors({3, 4})[2],
          "direction searches stop at the grid boundaries");
    check(!floor.connect({0, 0}, {3, 4}) && !floor.connect({0, 0}, {1, 0})
        && !floor.connect({0, 0}, {-1, 0}) && !floor.connect({0, 0}, {0, 0}),
        "invalid destinations cannot create a connection");

    auto blocked = floor;
    blocked.rooms[0][1] = Room{.id = 3};
    check(!blocked.connect({0, 0}, {3, 0}) && !blocked.neighbors({0, 0})[1]
        && !blocked.neighbors({3, 0})[3], "an intervening room blocks a longer connection");
    check(blocked.connect({0, 0}, {1, 0})
        && blocked.neighbors({0, 0})[1] == FloorPosition{1, 0},
        "a direction resolves to the nearest room");

    auto one_way = floor;
    one_way.rooms[0][0]->accessible_directions = RoomDirection::None;
    check(!one_way.neighbors({3, 0})[3], "long connections still require the reverse direction");

    using elysia::core::UiRenderCommand;
    using elysia::core::UiRenderCommandType;
    const auto can_enter = [](RoomID id) { return id == 1; };
    const auto choose_room = [](RoomID) {};
    FloorMapView preview(Rect{0, 0, 1280, 720}, floor, can_enter, choose_room,
                         FloorMapView::Visibility::ShowAll);
    std::vector<UiRenderCommand> commands;
    preview.submit_ui_render_commands(commands);
    const auto is_connection = [](const auto& command) {
        return command.type == UiRenderCommandType::DrawLine
            && command.color == elysia::core::colors::gray_500;
    };
    check(std::ranges::count_if(commands, is_connection) == 2,
          "sparse map draws horizontal and vertical long connections once");
    check(std::ranges::any_of(commands, [&](const auto& command) {
        return is_connection(command)
            && std::abs(command.line_end.x - command.line_start.x) > 300;
    }) && std::ranges::any_of(commands, [&](const auto& command) {
        return is_connection(command)
            && std::abs(command.line_end.y - command.line_start.y) > 300;
    }), "long lines span the empty grid cells");

    FloorMapView fogged(Rect{0, 0, 1280, 720}, floor, can_enter, choose_room);
    commands.clear();
    fogged.submit_ui_render_commands(commands);
    check(std::ranges::count_if(commands, is_connection) == 1,
          "long connections respect the hidden destination");
}

void check_skeleton(const Floor& floor, std::uint64_t seed, RoomID first_id)
{
    check(floor.seed == seed && floor.start_room_id == first_id
        && floor.end_room_id == first_id + 1 && floor.current_room_id == first_id,
        "generated floor metadata agrees with its seed and room IDs");

    const auto start = floor.find_position(first_id);
    const auto end = floor.find_position(first_id + 1);
    check(start && end, "both generated room IDs resolve to occupied cells");
    check(start->x >= 2 && start->x <= 3 && start->y >= 2 && start->y <= 3,
          "start occupies one of the central four cells");
    check(end->x == 0 || end->x == 5 || end->y == 0 || end->y == 5,
          "exit occupies one of the twenty perimeter cells");
    check(floor.room_at(*start)->type == RoomType::Start
        && floor.room_at(*end)->type == RoomType::End, "generated endpoints have the right types");

    int room_count = 0;
    for (int y = 0; y < Floor::height; ++y)
    {
        for (int x = 0; x < Floor::width; ++x)
        {
            const auto* room = floor.room_at({x, y});
            if (!room)
                continue;

            ++room_count;
            check(!room->discovered && !room->visited && !room->cleared && !room->event_triggered,
                  "generation initializes every room state");
            const auto neighbors = floor.neighbors({x, y});
            check(std::ranges::none_of(neighbors, [](const auto& neighbor) { return neighbor.has_value(); }),
                  "the skeleton contains no traversable connections");
        }
    }
    check(room_count == 2, "generation creates exactly two rooms");
}

void generated_floor_skeleton()
{
    const RoomFactory factory;
    const auto event = factory.create(17, RoomType::Event, RoomDirection::Left, "sealed_chest");
    check(event.id == 17 && event.type == RoomType::Event && event.content_id == "sealed_chest"
        && event.accessible_directions == RoomDirection::Left, "factory preserves the caller's room data");
    check(!event.discovered && !event.visited && !event.cleared && !event.event_triggered,
          "factory initializes exploration, combat and event state");
    check(factory.create_start(100).id == 100 && factory.create_end(101).id == 101
        && factory.create_test_room(102).id == 102, "factory helpers use explicit IDs");

    FloorGenerator generator;
    const auto first = generator.generate(42, 0);
    const auto offset = generator.generate(42, 100);
    (void)generator.generate(43, 500);
    const auto repeated = generator.generate(42, 0);
    check_skeleton(first, 42, 0);
    check_skeleton(offset, 42, 100);
    check_skeleton(repeated, 42, 0);
    for (int y = 0; y < Floor::height; ++y)
    {
        for (int x = 0; x < Floor::width; ++x)
        {
            const auto* original = first.room_at({x, y});
            const auto* shifted = offset.room_at({x, y});
            const auto* replay = repeated.room_at({x, y});
            check(bool(original) == bool(shifted) && bool(original) == bool(replay),
                  "room ID offsets and generator reuse preserve seeded positions");
            if (!original)
                continue;

            check(original->id == replay->id && shifted->id == original->id + 100
                && original->type == replay->type && original->type == shifted->type
                && original->accessible_directions == replay->accessible_directions
                && original->accessible_directions == shifted->accessible_directions,
                "repeated generation preserves room contents while IDs shift explicitly");
        }
    }
    for (std::uint64_t seed = 0; seed < 64; ++seed)
        check_skeleton(generator.generate(seed, 0), seed, 0);

    const auto max_id = std::numeric_limits<RoomID>::max();
    rejects([&] { (void)generator.generate(42, max_id); }, "room ID increment overflow is rejected");
    check_skeleton(generator.generate(42, max_id - 1), 42, max_id - 1);
    check_skeleton(generator.generate(42, 0), 42, 0);

    GameFlowManager flow;
    check(flow.get_current_room_id() == 0 && flow.get_current_room_type() == RoomType::Start,
          "constructor creates a valid starting room");
    for (const std::uint64_t seed : {42, 43, 42})
    {
        flow.mark_current_room_cleared();
        flow.restart(seed);
        const auto& floor = flow.current_floor();
        const auto* start = flow.find_room(0);
        const auto* end = flow.find_room(1);
        check(start && end && floor.start_room_id == 0 && floor.end_room_id == 1
            && floor.current_room_id == 0 && floor.seed == seed,
            "every restart resets room numbering and floor metadata");
        check(start->type == RoomType::Start && start->discovered && start->visited
            && !start->cleared && !start->event_triggered, "restart resets and visits the start");
        check(end->type == RoomType::End && end->discovered && !end->visited
            && !end->cleared && !end->event_triggered, "restart reveals the unvisited exit");
        check(flow.get_current_depth() == 0 && flow.room_seed(1) == seed + 1,
              "single-floor depth and room terrain seeds remain consistent");
        check(!flow.find_room(2) && !flow.can_enter_room(0) && !flow.can_enter_room(1)
            && !flow.can_enter_room(max_id) && !flow.try_enter_room(1),
            "unconnected exit and invalid IDs cannot be selected");
    }
}

void press_escape(elysia::scene::SceneManager& manager)
{
    using namespace elysia::input;
    RawInputState keyboard;
    for (const auto type : {RawInputEventType::ControlPressed, RawInputEventType::ControlReleased})
    {
        const RawInputEvent event{
            .control = RawInputControl::KeyEscape,
            .type = type,
            .device = InputDevice::Keyboard,
            .source = InputSourceId::keyboard(),
        };
        InputSnapshot snapshot;
        snapshot.sources = {{.source = event.source, .initial_state = keyboard}};
        apply_raw_event(keyboard, event);
        snapshot.sources.front().frame.state = keyboard;
        snapshot.events = {event};
        manager.on_input(snapshot);
    }
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
    manager.start({.target = MainMenu});

    const auto* query = ELYSIA_OBJECT_QUERY;
    for (int run = 0; run < 3; ++run)
    {
        click(manager, 640, 281); // Start below the logo.
        manager.on_update(0);
        check(manager.current_scene_key() == FixedRoom && flow.get_current_room_id() == 0
            && flow.get_current_room_type() == RoomType::Start,
            "menu Start repeatedly opens a valid starting room");
        const auto* start_map = query->find_object<DungeonRoom>();
        check(start_map && start_map->columns() == 15 && start_map->rows() == 15,
              "fixed start room dimensions");
        check(query->find_objects<Enemy>().empty(), "start room is safe");
        auto* player = query->find_object<PlayerCharacter>();
        const auto* portal = query->find_object<Portal>();
        check(player && portal && player->health() == initial_character.health,
              "each new run creates a fresh player and portal");
        player->set_position(portal->position());
        manager.on_update(0);
        check(manager.current_scene_key() == FloorMap, "start portal opens the skeleton map");

        const auto& floor = flow.current_floor();
        FloorMapView preview(Rect{0, 0, 1280, 720}, floor,
                             [&flow](RoomID id) { return flow.can_enter_room(id); }, [](RoomID) {});
        int button_count = 0;
        for (std::size_t i = 0; i < preview.child_count(); ++i)
        {
            const auto* button = dynamic_cast<const elysia::ui::UiButton*>(preview.child_at(i));
            if (!button)
                continue;

            ++button_count;
            check(!button->is_enabled(), "current start and unconnected exit buttons are disabled");
            const auto center = button->screen_rect().center();
            click(manager, static_cast<int>(center.x), static_cast<int>(center.y));
        }
        manager.on_update(0);
        check(button_count == 2 && manager.current_scene_key() == FloorMap
            && flow.get_current_room_id() == 0 && !flow.find_room(1)->visited,
            "both endpoints are visible and clicking them cannot leave the skeleton map");

        flow.mark_current_room_cleared();
        press_escape(manager);
        manager.on_update(0);
        check(manager.current_scene_key() == MainMenu, "map Escape returns to the menu for another Start");
    }

    click(manager, 1155, 38); // Top-right test entry.
    check(manager.current_scene_key() == Game && flow.get_current_room_id() == 0,
          "menu test still launches GameScene outside the run");
    press_escape(manager);
    manager.on_update(0);
    check(manager.current_scene_key() == MainMenu, "test scene returns to the menu");

    flow.restart(43);
    auto failed_route = make_run_room_route(flow.get_current_room_id(), initial_character);
    auto room = *elysia::scene::try_scene_payload<RoomScenePayload>(failed_route.payload);
    room.room.map_config.baseRoomSize = {2, 2};
    room.room.map_config.numSubRooms = 0;
    failed_route.payload = room;
    manager.on_scene_request({elysia::scene::SceneRequestType::Switch, failed_route});
    manager.on_update(0);
    check(manager.current_scene_key() != FixedRoom && flow.get_current_room_id() == 0
        && !flow.find_room(1)->visited, "failed room load leaves the generated floor untouched");
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
        generated_floor_skeleton();
        fixed_floor_topology();
        floor_map_directions();
        sparse_floor_connections();
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
