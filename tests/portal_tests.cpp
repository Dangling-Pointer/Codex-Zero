#include "scene_harness.h"
#include "game/objcets/portal.h"
#include "game/map/dungeon_room.h"
#include "game/map/room_placement.h"
#include "game/characters/character_manager.h"
#include "game/characters/player_character.h"
#include "game/characters/enemy.h"
#include "game/scene/combat_scene.h"
#include "game/scene/game_scene.h"
#include "engine/core/render/render_command.h"

#include <cmath>
#include <iostream>
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
    check(portal.center() == map.center() && portal.size() == Vector2{32, 32},
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
        check(portal->center() == map->center() && portal->size() == Vector2{32, 32}, "portal at map center");
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
        room_portal<GameScene>();
        room_portal<CombatScene>(RoomScenePayload{
            *CharacterManager::instance()->create_context("default_player"), {}});
        std::cout << "Portal tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
