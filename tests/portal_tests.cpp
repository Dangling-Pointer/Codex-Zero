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
#include "game/scene/run/corridor_scene.h"
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

void fixed_room_corridor_tests();
void floor_minimap_tests();
void gate_tests();

namespace
{
using elysia::core::Rect;
using elysia::core::Vector2;

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

template<class Exception = std::runtime_error, class Function> void rejects(Function&& function, const char* message)
{
    bool rejected = false;
    try { function(); }
    catch (const Exception&) { rejected = true; }
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
    config.baseRoomSize = {10, 10};
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

struct GeneratedFloorStats
{
    int room_count = 0;
    int directed_edges = 0;
    bool has_long_connection = false;
};

GeneratedFloorStats check_generated_floor(const Floor& floor, std::uint64_t seed,
                                         RoomID first_id, const FloorGenerationConfig& config)
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

    constexpr std::array<FloorPosition, 4> start_offsets{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
    const auto start_neighbors = floor.neighbors(*start);
    check(floor.room_at(*start)->accessible_directions == RoomDirection::All,
          "generated start has all four exits");
    for (std::size_t i = 0; i < start_offsets.size(); ++i)
    {
        const FloorPosition expected{start->x + start_offsets[i].x, start->y + start_offsets[i].y};
        check(start_neighbors[i] == expected, "start connects to its four immediately adjacent cells");
        const auto* neighbor = floor.room_at(expected);
        check(neighbor && neighbor->type == RoomType::Event
            && neighbor->id == static_cast<std::uint64_t>(first_id) + 2 + i,
            "starting event IDs follow up, right, down and left without being overwritten");
    }
    check(std::ranges::find(start_neighbors, end) == start_neighbors.end(),
          "exit never connects directly to the start");

    GeneratedFloorStats stats;
    std::vector<RoomID> ids;
    for (int y = 0; y < Floor::height; ++y)
    {
        for (int x = 0; x < Floor::width; ++x)
        {
            const FloorPosition position{x, y};
            const auto* room = floor.room_at(position);
            if (!room)
                continue;

            ++stats.room_count;
            ids.push_back(room->id);
            if (room->id != first_id && room->id != first_id + 1)
                check(room->type == RoomType::Event, "all additional rooms are event test rooms");
            check(!room->discovered && !room->visited && !room->cleared
                && !room->event_triggered && !room->content_id, "generation initializes every room state");

            for (const auto& neighbor : floor.neighbors(position))
            {
                if (!neighbor)
                    continue;

                ++stats.directed_edges;
                const auto reverse = floor.neighbors(*neighbor);
                check(std::ranges::find(reverse, std::optional{position}) != reverse.end(),
                      "generated connections are bidirectional");
                check(x == neighbor->x || y == neighbor->y, "connections stay on one row or column");
                const int dx = (neighbor->x > x) - (neighbor->x < x);
                const int dy = (neighbor->y > y) - (neighbor->y < y);
                FloorPosition between{x + dx, y + dy};
                while (between != *neighbor)
                {
                    stats.has_long_connection = true;
                    check(!floor.room_at(between), "long connections never skip an intervening room");
                    between.x += dx;
                    between.y += dy;
                }
            }
        }
    }
    check(stats.room_count >= config.min_room_number && stats.room_count <= config.max_room_number,
          "room count respects the configuration, including start and exit");
    check(stats.directed_edges == 2 * (stats.room_count - 1), "generated floor has exactly N minus one edges");
    std::ranges::sort(ids);
    for (std::size_t i = 0; i < ids.size(); ++i)
        check(ids[i] == static_cast<std::uint64_t>(first_id) + i, "room IDs are unique and contiguous");

    std::array<std::array<bool, Floor::width>, Floor::height> reached{};
    reached[start->y][start->x] = true;
    std::queue<FloorPosition> pending;
    pending.push(*start);
    int reachable = 0;
    while (!pending.empty())
    {
        const auto position = pending.front();
        pending.pop();
        ++reachable;
        for (const auto& neighbor : floor.neighbors(position))
        {
            if (neighbor && !reached[neighbor->y][neighbor->x])
            {
                reached[neighbor->y][neighbor->x] = true;
                pending.push(*neighbor);
            }
        }
    }
    check(reachable == stats.room_count, "every generated room is reachable from the start");
    const auto exit_neighbors = floor.neighbors(*end);
    check(std::ranges::count_if(exit_neighbors, [](const auto& neighbor) { return neighbor.has_value(); }) == 1,
          "exit has exactly one entrance");
    return stats;
}

void check_same_layout(const Floor& first, const Floor& second, RoomID id_offset = 0)
{
    check(first.seed == second.seed && second.start_room_id == first.start_room_id + id_offset
        && second.end_room_id == first.end_room_id + id_offset
        && second.current_room_id == first.current_room_id + id_offset, "seeded floor metadata repeats");
    for (int y = 0; y < Floor::height; ++y)
    {
        for (int x = 0; x < Floor::width; ++x)
        {
            const auto* original = first.room_at({x, y});
            const auto* replay = second.room_at({x, y});
            check(bool(original) == bool(replay), "seeded room positions repeat");
            if (original)
                check(replay->id == original->id + id_offset && replay->type == original->type
                    && replay->accessible_directions == original->accessible_directions
                    && replay->content_id == original->content_id, "seeded room data and connections repeat");
        }
    }
}

void generated_floor_tree()
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
    const FloorGenerationConfig config;
    const auto first = generator.generate(42, 0, config);
    const auto offset = generator.generate(42, 100, config);
    (void)generator.generate(43, 500, config);
    const auto repeated = generator.generate(42, 0, config);
    check_generated_floor(first, 42, 0, config);
    check_generated_floor(offset, 42, 100, config);
    check_generated_floor(repeated, 42, 0, config);
    check_same_layout(first, repeated);
    check_same_layout(first, offset, 100);

    bool found_long_connection = false;
    bool found_exit_bend = false;
    std::array<bool, 4> observed_counts{};
    for (std::uint64_t seed = 0; seed < 64; ++seed)
    {
        const auto floor = generator.generate(seed, 0, config);
        const auto stats = check_generated_floor(floor, seed, 0, config);
        found_long_connection |= stats.has_long_connection;
        observed_counts[stats.room_count - config.min_room_number] = true;
        for (int count : {6, 7, 12, 15, 36})
        {
            const FloorGenerationConfig fixed{count, count};
            const auto fixed_floor = generator.generate(seed, 100, fixed);
            const auto fixed_stats = check_generated_floor(fixed_floor, seed, 100, fixed);
            if (count == 36)
                check(!fixed_stats.has_long_connection, "full grid connections only join adjacent rooms");
            if (count == 6 || count == 7)
            {
                const auto start = *fixed_floor.find_position(100);
                const auto end = *fixed_floor.find_position(101);
                const bool shares_column = end.x >= start.x - 1 && end.x <= start.x + 1;
                const bool shares_row = end.y >= start.y - 1 && end.y <= start.y + 1;
                if (count == 6)
                    check(shares_column || shares_row, "six-room exit can attach without an extra room");
                else if (!shares_column && !shares_row)
                {
                    found_exit_bend = true;
                    const auto corner = *fixed_floor.find_position(106);
                    check(corner == FloorPosition{start.x, end.y} || corner == FloorPosition{end.x, start.y},
                          "seventh room provides a bend when the exit cannot see the cross");
                }
            }
        }
    }
    check(found_long_connection, "seeded generation produces connections across empty cells");
    check(found_exit_bend, "seven-room floors still support perimeter exits that require a bend");
    check(std::ranges::all_of(observed_counts, [](bool observed) { return observed; }),
          "fixed seed sample exercises all configured room counts");

    for (const FloorGenerationConfig invalid : {FloorGenerationConfig{-1, 15}, {3, 15}, {5, 15}, {15, 12}, {12, 37}})
        rejects<std::invalid_argument>([&] { (void)generator.generate(42, 0, invalid); },
                                       "invalid room count configuration is rejected");

    const auto max_id = std::numeric_limits<RoomID>::max();
    for (int count : {6, 7, 12, 15, 36})
    {
        const FloorGenerationConfig fixed{count, count};
        const RoomID first_id = max_id - count + 1;
        check_generated_floor(generator.generate(42, first_id, fixed), 42, first_id, fixed);
        rejects([&] { (void)generator.generate(42, first_id + 1, fixed); },
                "overflow anywhere in the full ID range is rejected");
    }
    check_same_layout(first, generator.generate(42, 0, config));
}

void generated_run_state()
{
    GameFlowManager flow;
    FloorGenerator generator;
    const FloorGenerationConfig config{13, 15};
    check(flow.get_current_room_id() == 0 && flow.get_current_room_type() == RoomType::Start,
          "constructor creates a valid starting room");
    for (const std::uint64_t seed : {42, 43, 42})
    {
        flow.restart(seed);
        const auto& floor = flow.current_floor();
        check_same_layout(generator.generate(seed, 0, config), floor);
        const auto start = *floor.find_position(floor.start_room_id);
        const auto neighbors = floor.neighbors(start);
        std::optional<RoomID> event_neighbor;
        int selectable_rooms = 0;
        for (int y = 0; y < Floor::height; ++y)
        {
            for (int x = 0; x < Floor::width; ++x)
            {
                const auto* room = floor.room_at({x, y});
                if (!room)
                    continue;

                const bool adjacent = std::ranges::find(neighbors, std::optional{FloorPosition{x, y}}) != neighbors.end();
                const bool is_start = room->id == floor.start_room_id;
                const bool is_end = room->id == floor.end_room_id;
                check(room->discovered == (is_start || is_end || adjacent) && room->visited == is_start,
                      "restart reveals the start, connected neighbors and exit");
                check(!room->cleared && !room->event_triggered, "restart resets combat and event state");
                check(flow.can_enter_room(room->id) == (adjacent && !is_start),
                      "only connected neighbors are initially selectable");
                if (flow.can_enter_room(room->id))
                    ++selectable_rooms;
                if (adjacent && room->type == RoomType::Event)
                    event_neighbor = room->id;
            }
        }
        check(selectable_rooms == 4 && !flow.can_enter_room(floor.end_room_id),
              "new run offers four starting choices and no direct exit");
        check(flow.get_current_depth() == 0 && flow.room_seed(2) == seed + 2,
              "single-floor depth and room terrain seeds remain consistent");
        check(!flow.can_enter_room(std::numeric_limits<RoomID>::max()), "invalid IDs cannot be selected");
        check(event_neighbor && flow.try_enter_room(*event_neighbor), "generated event neighbor is reachable");
        flow.mark_current_room_cleared();
        check(flow.try_enter_room(0) && flow.find_room(*event_neighbor)->cleared,
              "visited room travel preserves combat clear state until restart");
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

void choose_map_room(elysia::scene::SceneManager& manager, RoomID id, bool enabled = true)
{
    auto& flow = *GameFlowManager::instance();
    check(manager.current_scene_key() == FloorMap, "room selection starts on the map");
    FloorMapView preview(Rect{0, 0, 1280, 720}, flow.current_floor(),
                         [&flow](RoomID room_id) { return flow.can_enter_room(room_id); }, [](RoomID) {},
                         FloorMapView::Visibility::ShowAll);
    std::vector<elysia::core::UiRenderCommand> commands;
    preview.submit_ui_render_commands(commands);
    for (std::size_t i = 0; i < preview.child_count(); ++i)
    {
        const auto* button = dynamic_cast<const elysia::ui::UiButton*>(preview.child_at(i));
        if (!button || std::stoul(button->text_content().value) != id)
            continue;

        check(button->is_enabled() == enabled, "map button availability matches the generated route");
        const auto center = button->screen_rect().center();
        click(manager, static_cast<int>(center.x), static_cast<int>(center.y));
        manager.on_update(0);
        return;
    }
    throw std::runtime_error("Expected room button is missing from the map");
}

void enter_portal(elysia::scene::SceneManager& manager)
{
    const auto* query = ELYSIA_OBJECT_QUERY;
    auto* player = query->find_object<PlayerCharacter>();
    const auto* portal = query->find_object<Portal>();
    check(player && portal, "active room has a player and portal");
    player->set_position(portal->position());
    manager.on_update(0);
}

std::vector<TileType> terrain_layout(const DungeonRoom& map)
{
    std::vector<TileType> result;
    for (const auto& row : map.tile_map().data())
        for (const auto& tile : row)
            result.push_back(tile.type);
    return result;
}

std::vector<RoomID> path_to_exit(const Floor& floor)
{
    const auto start = *floor.find_position(floor.start_room_id);
    const auto end = *floor.find_position(floor.end_room_id);
    std::array<std::array<std::optional<FloorPosition>, Floor::width>, Floor::height> previous{};
    previous[start.y][start.x] = start;
    std::queue<FloorPosition> pending;
    pending.push(start);
    while (!pending.empty())
    {
        const auto position = pending.front();
        pending.pop();
        for (const auto& neighbor : floor.neighbors(position))
        {
            if (neighbor && !previous[neighbor->y][neighbor->x])
            {
                previous[neighbor->y][neighbor->x] = position;
                pending.push(*neighbor);
            }
        }
    }
    check(previous[end.y][end.x].has_value(), "generated exit has a route from the start");
    std::vector<RoomID> result;
    for (auto position = end; position != start; position = *previous[position.y][position.x])
        result.push_back(floor.room_at(position)->id);
    std::ranges::reverse(result);
    return result;
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
            "menu Start repeatedly opens a valid generated starting room");
        const auto* start_map = query->find_object<DungeonRoom>();
        check(start_map && start_map->columns() == 15 + 2 * kStandardRoomCorridors.horizontal_length
            && start_map->rows() == 15 + 2 * kStandardRoomCorridors.vertical_length,
              "fixed start keeps its 15 by 15 body with the shared directional corridor preset");
        check(query->find_objects<Enemy>().empty(), "start room is safe");
        auto* player = query->find_object<PlayerCharacter>();
        check(player && player->health() == initial_character.health && !flow.find_room(0)->cleared,
              "each new run creates fresh player and room state");
        enter_portal(manager);
        check(manager.current_scene_key() == FloorMap, "start portal opens the generated map");
        choose_map_room(manager, 0, false);
        choose_map_room(manager, 1, false);
        check(manager.current_scene_key() == FloorMap && flow.get_current_room_id() == 0,
              "current room and unreachable exit buttons cannot change rooms");

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

    // Event test rooms exercise map selection, state transfer and revisits independently of combat.
    flow.restart(42);
    manager.on_scene_request({elysia::scene::SceneRequestType::Switch,
                             make_run_room_route(flow.get_current_room_id(), initial_character)});
    manager.on_update(0);
    enter_portal(manager);
    const auto& floor = flow.current_floor();
    const RoomID first_event = 2;
    choose_map_room(manager, first_event);
    check(manager.current_scene_key() == FixedRoom && flow.get_current_room_id() == first_event,
          "map selection enters the generated event room");
    const auto layout = terrain_layout(*query->find_object<DungeonRoom>());
    check(query->find_objects<Enemy>().empty(), "event test rooms have no enemies");
    auto* player = query->find_object<PlayerCharacter>();
    player->receive_attack({0, 0, 10});
    const float health = player->health();
    enter_portal(manager);
    choose_map_room(manager, 0);
    check(query->find_object<PlayerCharacter>()->health() == health, "player state survives return to start");
    enter_portal(manager);
    choose_map_room(manager, first_event);
    check(terrain_layout(*query->find_object<DungeonRoom>()) == layout,
          "event revisit preserves corridor terrain");
    check(query->find_object<PlayerCharacter>()->health() == health, "player state survives event revisit");
    enter_portal(manager);
    choose_map_room(manager, 0);
    enter_portal(manager);

    for (RoomID id : path_to_exit(floor))
    {
        choose_map_room(manager, id);
        check(flow.get_current_room_id() == id && query->find_object<PlayerCharacter>()->health() == health,
              "generated route commits movement and retains player state");
        check(manager.current_scene_key() == FixedRoom, "generated Event and End rooms route to fixed rooms");
        enter_portal(manager);
    }
    check(manager.current_scene_key() == MainMenu, "generated exit portal returns to the main menu");
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

void corridor_movement_and_lifecycle()
{
    const auto character = *CharacterManager::instance()->create_context("default_player");
    const auto route = make_corridor_test_route(character);
    const auto* payload = elysia::scene::try_scene_payload<RoomScenePayload>(route.payload);
    check(route.target == Corridor && payload && !payload->room_id && !payload->from_room,
          "corridor route is independent of dungeon room routing");
    test::SceneHarness<CorridorScene> host(route.payload);
    auto& scene = host.scene();
    using Key = elysia::input::RawInputControl;

    const auto drive = [&](Key key, int ticks) {
        elysia::input::RawInputFrame frame;
        frame.state.set_pressed(key, true);
        host.input(frame, {});
        for (int tick = 0; tick < ticks; ++tick)
            host.update(1.0 / 60.0);
    };

    for (int visit = 0; visit < 3; ++visit)
    {
        if (visit)
            host.enter(route.payload, visit == 1 ? elysia::scene::SceneReloadMode::Reuse
                                                : elysia::scene::SceneReloadMode::Reset);
        const auto maps = objects<DungeonRoom>(scene);
        const auto players = objects<PlayerCharacter>(scene);
        check(maps.size() == 1 && players.size() == 1 && objects<Portal>(scene).empty()
            && objects<Enemy>(scene).empty(), "corridor has one map and player, with no portal or enemies");
        auto* map = maps.front();
        auto* player = players.front();
        check(player->physics_world() != nullptr, "corridor player is bound to physics");
        auto& world = *player->physics_world();
        check(map->columns() == payload->room.grid_size.x && map->rows() == payload->room.grid_size.y, "corridor uses the configured standalone layout");
        check(game::walkable_bounds(*map, player->world_rect()), "corridor player spawns on floor");
        check(std::abs(player->center().y - map->center().y) < 0.01f
            && player->center().x < map->center().x,
            "corridor spawn is centered vertically on the left side");
        check(scene.camera().zoom() == 2.0f, "corridor keeps the room camera zoom");
        check(world.registered_object_count() == 1,
              "corridor reentry has no leftover physics objects");

        const auto spawn = player->position();
        const auto camera_start = scene.camera().center();
        drive(Key::KeyD, 20);
        check(player->position().x > spawn.x + 40 && player->velocity().x > 0,
              "corridor player responds to its bound movement controller");
        check(scene.camera().center().x > camera_start.x, "corridor camera follows movement");

        const auto tile = map->tile_size();
        const Rect interior{map->position() + tile, map->size() - tile * 2};
        const auto local_body = std::get<elysia::physics::BoxShape>(
            player->collider_definitions().front().shape).local_bounds();
        for (const auto key : {Key::KeyA, Key::KeyD, Key::KeyW, Key::KeyS})
        {
            auto test_spawn = spawn;
            if (key == Key::KeyA)
                test_spawn.x = interior.left() + 8;
            if (key == Key::KeyD)
                test_spawn.x = interior.right() - player->size().x - 8;
            check(world.teleport_object(player->physics_handle(), test_spawn,
                    elysia::physics::TeleportVelocityMode::Clear), "corridor collision test teleports player");
            drive(key, 90);
            const Rect body{player->position() + local_body.position(), local_body.size()};
            constexpr float tolerance = 1.0f;
            check(body.left() >= interior.left() - tolerance && body.right() <= interior.right() + tolerance
                && body.top() >= interior.top() - tolerance && body.bottom() <= interior.bottom() + tolerance,
                "corridor walls contain the player's collision body");
            float gap = 0;
            switch (key)
            {
            case Key::KeyA: gap = body.left() - interior.left(); break;
            case Key::KeyD: gap = interior.right() - body.right(); break;
            case Key::KeyW: gap = body.top() - interior.top(); break;
            case Key::KeyS: gap = interior.bottom() - body.bottom(); break;
            default: throw std::runtime_error("Unexpected corridor wall test direction");
            }
            check(std::abs(gap) < tolerance, "each corridor wall stops an actual movement attempt");
        }
        host.leave();
        check(objects<PlayerCharacter>(scene).empty() && objects<DungeonRoom>(scene).empty()
            && world.registered_object_count() == 0,
            "leaving corridor releases objects and physics registrations");
    }
}

void corridor_menu_entry()
{
    auto& flow = *GameFlowManager::instance();
    const Floor original = flow.current_floor();
    elysia::io::ContentRegistry content;
    elysia::scene::SceneRuntimeContext context{nullptr, content, 1280, 720};
    elysia::scene::SceneManager manager;
    manager.initialize(context);
    GameModule{}.register_scenes(manager);
    manager.start({.target = MainMenu});

    for (int visit = 0; visit < 2; ++visit)
    {
        click(manager, 125, 38); // Existing top-left character selection button.
        manager.on_update(0);
        check(manager.current_scene_key() == Corridor, "character selection button opens corridor test");
        const auto* query = ELYSIA_OBJECT_QUERY;
        const auto maps = query->find_objects<DungeonRoom>();
        check(maps.size() == 1 && maps.front()->columns() == 60 && maps.front()->rows() == 7,
              "menu corridor entry uses its configured fixed layout");
        check(query->find_objects<PlayerCharacter>().size() == 1
            && query->find_objects<Portal>().empty() && query->find_objects<Enemy>().empty(),
            "repeated menu entry creates only the standalone corridor content");
        press_escape(manager);
        manager.on_update(0);
        check(manager.current_scene_key() == MainMenu, "corridor Escape returns to the main menu");
    }

    const auto& after = flow.current_floor();
    check_same_layout(original, after);
    for (int y = 0; y < Floor::height; ++y)
    {
        for (int x = 0; x < Floor::width; ++x)
        {
            const auto* before = original.room_at({x, y});
            if (!before)
                continue;
            const auto* room = after.room_at({x, y});
            check(room->discovered == before->discovered && room->visited == before->visited
                && room->cleared == before->cleared && room->event_triggered == before->event_triggered,
                "corridor test leaves dungeon exploration and completion unchanged");
        }
    }
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
        gate_tests();
        placement();
        fixed_room_corridor_tests();
        floor_minimap_tests();
        room_portal<CombatScene>(RoomScenePayload{
            *CharacterManager::instance()->create_context("default_player"), {}});
        generated_floor_tree();
        generated_run_state();
        fixed_floor_topology();
        floor_map_directions();
        sparse_floor_connections();
        single_floor_run();
        corridor_movement_and_lifecycle();
        corridor_menu_entry();
        std::cout << "Portal tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
