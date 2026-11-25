#include "scene_harness.h"
#include "game/map/dungeon_room.h"
#include "game/map/room_placement.h"
#include "game/map/room_query.h"
#include "game/map/room_service.h"
#include "game/characters/character_manager.h"
#include "game/characters/player_character.h"
#include "game/scene/game_scene.h"
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
    const RoomQuery query(map);
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
        && !query.tile_at({query.map_bounds().right(), origin.y + 32})
        && !query.tile_at({origin.x + 32, query.map_bounds().bottom()}),
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
    const RoomQuery query(map, exits);
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
    check(RoomQuery(flat, strips).can_place({96, 96, 64, 32}, {.region = SpawnRegion::Corridor}),
          "full corridor coverage accepts a rectangle spanning touching layouts");
    check(!RoomQuery(flat, strips).can_place({96, 96, 65, 32}, {.region = SpawnRegion::Corridor}),
          "one uncovered pixel rejects corridor-only placement");
}

void invalid_inputs_and_compatibility()
{
    DungeonRoom map(fixed_config(), {8, 8});
    const RoomQuery query(map);
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    check(!query.tile_at({nan, 0}) && !query.can_place({nan, 32, 32, 32})
        && !query.can_place({32, 32, 0, 32})
        && !query.find_spawn({nan, 32}, map.center())
        && !query.find_spawn({0, 32}, map.center())
        && !query.find_spawn({32, 32}, {nan, 0}), "invalid positions and sizes fail without searching");
    const Rect occupied = map.world_rect();
    check(!query.find_spawn({32, 32}, map.center(), {.occupied = {&occupied, 1}}),
          "no free position returns nullopt");
    check(find_room_spawn(map, {32, 32}, map.center(), {}) == *query.find_spawn({32, 32}, map.center()),
          "legacy spawn entry delegates to identical search");
    bool threw = false;
    try { (void)find_room_spawn(map, {32, 32}, map.center(), {&occupied, 1}); }
    catch (const std::runtime_error&) { threw = true; }
    check(threw, "legacy no-space behavior still throws");
    check(walkable_bounds(map, {32, 32, 32, 32}) == query.is_walkable({32, 32, 32, 32}),
          "legacy terrain entry shares query implementation");

    DungeonRoom random(MapConfig{.seed = 42}, {50, 50});
    const RoomQuery random_query(random);
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
    check(!service.current(), "no room service binding before scenes");
    DungeonRoom map(fixed_config(), {8, 8});
    const RoomQuery first(map), second(map);
    check(service.bind(first) && service.bind(first) && service.current() == &first,
          "same query binding is idempotent");
    check(!service.bind(second) && !service.unbind(second) && service.current() == &first,
          "foreign bind and unbind preserve active query");
    check(service.unbind(first) && !service.current(), "matching unbind clears global pointer");
}

template<class Base> class QueryProbe : public Base
{
public:
    inline static bool fail_content = false;
    const RoomQuery* content_query = nullptr;
protected:
    void create_room_content() override
    {
        content_query = &this->room_query();
        check(!RoomService::instance()->current(), "query is local until scene construction completes");
        check(content_query->is_walkable(this->player()->world_rect()), "content creation can query terrain");
        Base::create_room_content();
        if (fail_content)
            throw std::runtime_error("Injected room content failure");
    }
};

template<class Base> void scene_lifecycle(const RoomScenePayload& payload, bool corridor_only)
{
    auto& service = *RoomService::instance();
    {
        test::SceneHarness<QueryProbe<Base>> host(payload);
        for (int visit = 0; visit < 3; ++visit)
        {
            if (visit)
                host.enter(payload, visit == 1 ? elysia::scene::SceneReloadMode::Reuse
                                              : elysia::scene::SceneReloadMode::Reset);
            const auto* query = service.current();
            check(query && query == host.scene().content_query, "successful entry exposes the scene owned query");
            const auto center = query->map_bounds().center();
            check(query->region_at(center).region == (corridor_only ? RoomRegion::Corridor : RoomRegion::Room),
                  "scene query uses its correct region mode");
            if (corridor_only)
            {
                check(query->can_place(Rect::from_center(center, {32, 32}), {.region = SpawnRegion::Corridor})
                    && !query->find_spawn({32, 32}, center, {.region = SpawnRegion::Room}),
                      "standalone corridor middle is fully corridor terrain");
            }
            host.leave();
            check(!service.current(), "leaving to a scene without a room clears service");
        }
        host.enter(payload);
    }
    check(!service.current(), "manager destruction clears the room query");
}

void failed_entry_cleanup(const RoomScenePayload& payload)
{
    test::SceneHarness<QueryProbe<FixedRoomScene>> host(payload);
    host.leave();
    QueryProbe<FixedRoomScene>::fail_content = true;
    host.manager.on_scene_request({elysia::scene::SceneRequestType::Switch, {.target = 1, .payload = payload}});
    host.manager.on_update(0);
    QueryProbe<FixedRoomScene>::fail_content = false;
    check(host.manager.state() != elysia::scene::SceneManagerState::Running && !RoomService::instance()->current(),
          "content construction failure leaves no global query");
}

void conflicting_entry(const RoomScenePayload& payload)
{
    DungeonRoom map(fixed_config(), {8, 8});
    const RoomQuery foreign(map);
    auto& service = *RoomService::instance();
    check(service.bind(foreign), "bind foreign query for conflict test");
    bool rejected = false;
    try { test::SceneHarness<FixedRoomScene> host(payload); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected && service.current() == &foreign,
          "failed scene publication and cleanup cannot clear another owner's query");
    check(service.unbind(foreign), "release foreign query after entry conflict");
}
}

void room_query_tests()
{
    terrain_and_placement();
    regions_and_corridors();
    invalid_inputs_and_compatibility();
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
        check(!RoomService::instance()->current(), "GameScene remains outside RoomService");
    }
}
