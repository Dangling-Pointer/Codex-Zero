#include "scene_harness.h"
#include "game/map/dungeon_room.h"
#include "game/map/runtime/room_manager.h"
#include "game/map/room_service.h"
#include "game/characters/character_manager.h"
#include "game/characters/player_character.h"
#include "game/scene/game_scene.h"
#include "game/scene/run/transition_scene.h"
#include "game/characters/enemy.h"
#include "game/gameflow/game_flow_manager.h"
#include "game/objcets/portal.h"
#include "game/scene/scene_keys.h"
#include "engine/object_query/game_object_query_service.h"
#include "game/scene/run/combat_scene.h"
#include "game/scene/run/corridor_scene.h"
#include "game/scene/run/fixed_room_scene.h"

#include <array>
#include <limits>
#include <stdexcept>

namespace
{
using elysia::core::Rect;
using elysia::core::Vector2;
using namespace game;

void check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

MapConfig fixed_config()
{
    return {.baseRoomSize = {8, 8}, .numSubRooms = 0, .seed = 7};
}

void terrain_and_placement()
{
    DungeonRoom map(fixed_config(), {8, 8});
    map.set_position({-96, 160});
    RoomManager query;
    query.bind_map(map);
    const auto origin = map.world_origin();
    const Vector2 first = origin + Vector2{32, 32};
    const Rect ground{first, {32, 32}};
    check(query.map_bounds() == Rect{origin, {256, 256}}, "query uses translated map bounds");
    check(query.tile_at(first) == TileType::Floor && query.is_walkable(ground),
          "translated world position maps to floor cell");
    check(query.region_at(ground.center()).region == RoomRegion::Room,
          "floor in a fixed map is a room");
    check(query.tile_at(origin) && query.tile_at(origin) != TileType::Floor
        && query.region_at(origin).region == RoomRegion::None,
          "wall tile is queryable but has no walkable region");
    check(!query.tile_at(origin - Vector2{0.01f, 0})
        && !query.tile_at({query.map_bounds()->right(), origin.y + 32})
        && !query.tile_at({origin.x + 32, query.map_bounds()->bottom()}),
          "tile conversion rejects negative local positions and exclusive far bounds");
    check(!query.is_walkable({origin, {64, 64}})
        && !query.is_walkable({origin - Vector2{1, 0}, {32, 32}}),
          "complete object bounds reject walls and outside terrain");

    const std::array<Rect, 1> occupied{ground};
    check(!query.can_place(ground, {.occupied = occupied}), "explicit occupied rectangle blocks placement");
    check(query.can_place({first + Vector2{32, 0}, {32, 32}}, {.occupied = occupied}),
          "touching occupied edges does not count as overlap");
    check(query.find_spawn({32, 32}, ground.center()) == first,
          "spawn returns top left nearest to preferred center");
    check(query.find_spawn({32, 32}, ground.center() + Vector2{16, 16}) == first,
          "equal distance candidates retain y then x ordering");
    check(query.find_spawn({32, 32}, ground.center(), {.occupied = occupied}) == first + Vector2{32, 0},
          "spawn skips occupied candidate and keeps deterministic tie order");
    check(!query.find_spawn({10000, 10000}, map.center()), "oversized object has no spawn");
}

void regions_and_corridors()
{
    auto config = fixed_config();
    config.baseRoomSize = {15, 15};
    config.corridors.directions = RoomDirection::All;
    DungeonRoom map(config, config.baseRoomSize + config.corridors.padding() * 2);
    map.set_position({128, -64});
    const auto exits = map.corridors();
    RoomManager query;
    query.bind_map(map, exits);
    check(query.region_at(map.center()).region == RoomRegion::Room, "body stays a room");
    for (const auto& exit : exits)
    {
        const auto location = query.region_at(exit.seam_center());
        check(location.region == RoomRegion::Corridor && location.direction == exit.direction,
              "all four corridors report their direction");
        check(query.corridor(exit.direction) == &exit, "query borrows the existing world layout");
        const Rect inside = Rect::from_center(exit.seam_center(), {32, 32});
        check(query.can_place(inside, {.region = SpawnRegion::Corridor})
            && !query.can_place(inside, {.region = SpawnRegion::Room}),
              "spawn region filters distinguish room from corridor");
        const auto spawn = query.find_spawn({32, 32}, exit.seam_center(), {.region = SpawnRegion::Corridor});
        check(spawn && query.can_place({*spawn, {32, 32}}, {.region = SpawnRegion::Corridor}),
              "corridor restricted spawn searches actual passage terrain");
    }
    check(!query.corridor(RoomDirection::None), "unknown exit returns null");
    const auto& right = *query.corridor(RoomDirection::Right);
    const Rect crossing{right.walkable.left() - 16, right.walkable.center().y - 16, 32, 32};
    check(query.can_place(crossing)
        && !query.can_place(crossing, {.region = SpawnRegion::Room})
        && !query.can_place(crossing, {.region = SpawnRegion::Corridor}),
          "whole rectangle crossing the mouth cannot pass a single region restriction");
    const auto room_spawn = query.find_spawn({32, 32}, right.seam_center(), {.region = SpawnRegion::Room});
    check(room_spawn && query.can_place({*room_spawn, {32, 32}}, {.region = SpawnRegion::Room}),
          "room restricted search never falls back to a passage");
    check(query.tile_at(map.world_origin()) == TileType::Void
        && query.region_at(map.world_origin()).region == RoomRegion::None,
          "empty padding is void rather than a room");

    DungeonRoom flat(fixed_config(), {8, 8});
    const std::array<RoomCorridorLayout, 2> strips{{
        {RoomDirection::Right, {96, 96, 32, 32}, {96, 96, 32, 32}},
        {RoomDirection::Down, {128, 96, 32, 32}, {128, 96, 32, 32}},
    }};
    RoomManager combined;
    combined.bind_map(flat, strips);
    check(combined.can_place({96, 96, 64, 32}, {.region = SpawnRegion::Corridor}),
          "full corridor coverage accepts a rectangle spanning touching layouts");
    check(!combined.can_place({96, 96, 65, 32}, {.region = SpawnRegion::Corridor}),
          "one uncovered pixel rejects corridor-only placement");
}

void invalid_inputs()
{
    DungeonRoom map(fixed_config(), {8, 8});
    RoomManager query;
    query.bind_map(map);
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    check(!query.tile_at({nan, 0}) && !query.can_place({nan, 32, 32, 32})
        && !query.can_place({32, 32, 0, 32})
        && !query.find_spawn({nan, 32}, map.center())
        && !query.find_spawn({0, 32}, map.center())
        && !query.find_spawn({32, 32}, {nan, 0}), "invalid positions and sizes fail without searching");
    const Rect occupied = map.world_rect();
    check(!query.find_spawn({32, 32}, map.center(), {.occupied = {&occupied, 1}}),
          "no free position returns nullopt");
    DungeonRoom random(MapConfig{.seed = 42}, {50, 50});
    RoomManager random_query;
    random_query.bind_map(random);
    check(random_query.region_at(random.center()).region == RoomRegion::Room,
          "random combat map has a room region");
    const auto spawn = random_query.find_spawn({32, 32}, random.center(), {.region = SpawnRegion::Room});
    check(spawn && random_query.is_walkable({*spawn, {32, 32}}), "random combat room supports safe placement");
    check(!random_query.find_spawn({32, 32}, random.center(), {.region = SpawnRegion::Corridor}),
          "random map without exits has no corridor spawn");
}

void service_binding()
{
    auto& service = *RoomService::instance();
    const Rect ground{32, 32, 32, 32};
    check(!service.map_bounds() && !service.tile_at({48, 48})
        && service.region_at({48, 48}).region == RoomRegion::None
        && !service.corridor(RoomDirection::Right) && !service.is_walkable(ground)
        && !service.can_place(ground) && !service.find_spawn({32, 32}, {48, 48}),
          "all service queries have defined empty results without a room");
    DungeonRoom map(fixed_config(), {8, 8});
    DungeonRoom other(fixed_config(), {10, 10});
    RoomManager first, second;
    check(!first.activate(), "unbound manager cannot activate");
    first.bind_map(map);
    second.bind_map(other);
    check(!service.map_bounds() && first.is_walkable(ground), "local manager works before publication");
    check(first.activate() && first.activate() && service.map_bounds() == map.world_rect(),
          "activation publishes the map and is idempotent");
    check(!second.activate(), "another manager cannot replace the active room");
    second.clear();
    check(service.map_bounds() == map.world_rect(), "foreign clear preserves active manager");
    check(service.tile_at({48, 48}) == first.tile_at({48, 48})
        && service.can_place(ground) && service.find_spawn({32, 32}, {48, 48}) == Vector2{32, 32},
          "service forwards live queries");
    first.clear();
    check(!service.map_bounds() && !first.map_bounds(), "clear resets manager and service");
    second.bind_map(other);
    check(second.activate() && service.map_bounds() == other.world_rect(), "new manager can publish after clear");
}

template<class Base> class RuntimeProbe : public Base
{
public:
    inline static bool fail_content = false;
    const RoomManager* content_manager = nullptr;
    void destroy_map() { const_cast<elysia::core::GameObject*>(this->room_map())->destroy(); }
protected:
    void create_room_content() override
    {
        content_manager = &this->room_manager();
        check(!RoomService::instance()->map_bounds(), "runtime is local until construction completes");
        check(content_manager->is_walkable(this->player()->world_rect()), "content initialization can query terrain");
        Base::create_room_content();
        if (fail_content)
            throw std::runtime_error("Injected room content failure");
    }
};

template<class Base> void scene_lifecycle(const RoomScenePayload& payload, bool corridor_only)
{
    auto& service = *RoomService::instance();
    {
        test::SceneHarness<RuntimeProbe<Base>> host(payload);
        for (int visit = 0; visit < 3; ++visit)
        {
            if (visit)
                host.enter(payload, visit == 1 ? elysia::scene::SceneReloadMode::Reuse
                                              : elysia::scene::SceneReloadMode::Reset);
            check(service.map_bounds() == host.scene().content_manager->map_bounds(),
                  "successful entry publishes its own runtime");
            const auto center = service.map_bounds()->center();
            check(service.region_at(center).region == (corridor_only ? RoomRegion::Corridor : RoomRegion::Room),
                  "scene runtime reports correct region mode");
            if (corridor_only)
                check(!service.find_spawn({32, 32}, center, {.region = SpawnRegion::Room}),
                      "standalone corridor has no room spawn region");
            host.leave();
            check(!service.map_bounds(), "scene exit clears runtime");
        }
        host.enter(payload);
        host.scene().destroy_map();
        host.update(0);
        check(!service.map_bounds(), "early map removal clears runtime");
        host.leave();
        host.enter(payload);
        check(service.map_bounds().has_value(), "entry after map removal rebinds runtime");
    }
    check(!service.map_bounds(), "scene destruction clears runtime");
}

void failed_entry_cleanup(const RoomScenePayload& payload)
{
    test::SceneHarness<RuntimeProbe<FixedRoomScene>> host(payload);
    host.leave();
    RuntimeProbe<FixedRoomScene>::fail_content = true;
    host.manager.on_scene_request({elysia::scene::SceneRequestType::Switch, {.target = 1, .payload = payload}});
    host.manager.on_update(0);
    RuntimeProbe<FixedRoomScene>::fail_content = false;
    check(host.manager.state() != elysia::scene::SceneManagerState::Running && !RoomService::instance()->map_bounds(),
          "construction failure leaves no published runtime");
}

void conflicting_entry(const RoomScenePayload& payload)
{
    DungeonRoom map(fixed_config(), {8, 8});
    RoomManager foreign;
    foreign.bind_map(map);
    check(foreign.activate(), "activate foreign runtime");
    bool rejected = false;
    try { test::SceneHarness<FixedRoomScene> host(payload); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected && RoomService::instance()->map_bounds() == map.world_rect(),
          "failed scene publication cannot clear foreign runtime");
}
}

void room_runtime_tests()
{
    terrain_and_placement();
    regions_and_corridors();
    invalid_inputs();
    service_binding();
    const auto character = *CharacterManager::instance()->create_context("default_player");
    RoomScenePayload fixed{.character = character,
                           .room = {.map_config = fixed_config(), .grid_size = {8, 8}}};
    scene_lifecycle<FixedRoomScene>(fixed, false);
    scene_lifecycle<CombatScene>(RoomScenePayload{.character = character}, false);
    const auto route = make_corridor_test_route(character);
    scene_lifecycle<CorridorScene>(*elysia::scene::try_scene_payload<RoomScenePayload>(route.payload), true);
    failed_entry_cleanup(fixed);
    conflicting_entry(fixed);
    {
        test::SceneHarness<GameScene> host;
        check(!RoomService::instance()->map_bounds(), "GameScene stays outside room runtime");
    }
}

namespace
{
void map_key(elysia::scene::SceneManager& manager)
{
    using namespace elysia::input;
    RawInputEvent event{
        .control = RawInputControl::KeyM,
        .type = RawInputEventType::ControlPressed,
        .device = InputDevice::Keyboard,
        .source = InputSourceId::keyboard(),
    };
    manager.on_input(InputSnapshot{.sources = {{.source = event.source}}, .events = {event}});
    manager.on_update(0);
}

void portal_and_combat_flow()
{
    auto& flow = *GameFlowManager::instance();
    flow.restart(42);
    struct RestoreFloor { ~RestoreFloor() { GameFlowManager::instance()->restart(42); } } restore;
    auto& floor = const_cast<Floor&>(flow.current_floor());
    floor = Floor{.start_room_id = 0, .end_room_id = 2, .current_room_id = 0, .seed = 42};
    floor.rooms[2][2] = Room{.id = 0, .type = RoomType::Start};
    floor.rooms[2][3] = Room{.id = 1, .type = RoomType::Combat};
    floor.rooms[2][4] = Room{.id = 2, .type = RoomType::End};
    floor.rooms[2][2]->visited = true;
    floor.rooms[2][2]->discovered = true;
    check(floor.connect({2, 2}, {3, 2}) && floor.connect({3, 2}, {4, 2}),
          "construct start combat exit test floor");

    const auto character = *CharacterManager::instance()->create_context("default_player");
    elysia::io::ContentRegistry content;
    elysia::scene::SceneRuntimeContext context{nullptr, content, 1280, 720};
    elysia::scene::SceneManager manager;
    manager.initialize(context);
    auto* controllers = elysia::gameplay::ControllerService::instance();
    if (!controllers->session_active())
        check(controllers->begin_session().has_value(), "start controller session for flow tests");
    FixedRoomScene* fixed_scene = nullptr;
    CombatScene* combat_scene = nullptr;
    manager.register_game_scene<test::ObservedScene<FixedRoomScene>>(FixedRoom, std::ref(fixed_scene));
    manager.register_game_scene<test::ObservedScene<CombatScene>>(CombatRoom, std::ref(combat_scene));
    manager.register_game_scene<TransitionScene>(FloorMap);
    manager.register_game_scene<test::EmptyScene>(MainMenu);
    manager.start(make_run_room_route(0, character));
    manager.on_update(0);
    auto* objects = ELYSIA_OBJECT_QUERY;
    auto* player = objects->find_object<PlayerCharacter>();
    check(player && objects->find_objects<Portal>().empty(), "start has a player and no portal");
    const auto route = [&](RoomID destination) {
        manager.on_scene_request({elysia::scene::SceneRequestType::Switch,
            make_run_room_route(destination, character, flow.get_current_room_id())});
        manager.on_update(0);
        manager.on_update(0); // Commit entry only after construction succeeds.
        check(manager.state() == elysia::scene::SceneManagerState::Running
            && flow.get_current_room_id() == destination, "room route commits requested destination");
    };

    player->set_active(false);
    map_key(manager);
    check(manager.current_scene_key() == FixedRoom, "inactive player cannot open map");
    player->set_active(true);
    auto* start_scene = fixed_scene;
    check(start_scene != nullptr, "player belongs to the start room");
    start_scene->pause();
    map_key(manager);
    check(manager.current_scene_key() == FixedRoom, "paused room cannot open map");
    start_scene->resume();
    player->receive_attack({0, 0, player->max_health()});
    map_key(manager);
    check(manager.current_scene_key() == FixedRoom, "dead player cannot open map");
    manager.on_scene_request({elysia::scene::SceneRequestType::Switch, make_run_room_route(0, character)});
    manager.on_update(0);
    map_key(manager);
    check(manager.current_scene_key() == FloorMap, "M opens map from a live ordinary room");
    check(!RoomService::instance()->map_bounds(), "map scene has no room runtime");

    route(1);
    check(manager.current_scene_key() == CombatRoom && !objects->find_objects<Enemy>().empty()
        && objects->find_objects<Portal>().empty(), "combat creates enemies with no portal");
    map_key(manager);
    check(manager.current_scene_key() == CombatRoom && !flow.find_room(1)->cleared,
          "M cannot bypass uncleared combat");
    player = objects->find_object<PlayerCharacter>();

    combat_scene->pause();
    for (auto* enemy : objects->find_objects<Enemy>())
        enemy->receive_attack({0, 0, enemy->max_health()});
    manager.on_update(0);
    check(!flow.find_room(1)->cleared, "paused update does not commit clear state");
    combat_scene->resume();
    manager.on_update(0);
    check(flow.find_room(1)->cleared, "last enemy death marks room cleared without portal or map input");
    map_key(manager);
    check(manager.current_scene_key() == FloorMap, "cleared combat allows M");
    route(0);
    map_key(manager);
    route(1);
    check(objects->find_objects<Enemy>().empty(), "cleared combat revisit creates no enemies");
    map_key(manager);
    route(2);
    auto portals = objects->find_objects<Portal>();
    player = objects->find_object<PlayerCharacter>();
    const auto* map = objects->find_object<DungeonRoom>();
    check(portals.size() == 1 && portals.front()->size() == Vector2{62, 62}
        && portals.front()->center() == map->center()
        && !portals.front()->world_rect().intersects(player->world_rect()),
          "exit has one central portal and a separate safe player spawn");
    map_key(manager);
    check(manager.current_scene_key() == FloorMap, "exit room also supports M");
    route(1);
    map_key(manager);
    route(2);
    player = objects->find_object<PlayerCharacter>();
    auto* portal = objects->find_object<Portal>();
    portal->set_active(false);
    player->set_position(portal->position());
    manager.on_update(0);
    check(manager.current_scene_key() == FixedRoom, "inactive exit portal cannot trigger");
    portal->set_active(true);
    manager.on_update(0);
    check(manager.current_scene_key() == MainMenu && !RoomService::instance()->map_bounds(),
          "exit portal returns to menu and releases runtime");
}

void corridor_map_shortcut()
{
    const auto character = *CharacterManager::instance()->create_context("default_player");
    test::SceneHarness<CorridorScene> host(make_corridor_test_route(character).payload);
    map_key(host.manager);
    check(host.manager.current_scene_key() == 1, "standalone corridor ignores M");
}
}

void room_flow_tests()
{
    portal_and_combat_flow();
    corridor_map_shortcut();
}
