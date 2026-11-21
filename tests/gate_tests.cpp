#include "scene_harness.h"
#include "game/objcets/gate.h"
#include "game/objcets/portal.h"
#include "game/scene/run/fixed_room_scene.h"
#include "game/map/dungeon_room.h"
#include "game/map/room_placement.h"
#include "game/characters/character_manager.h"
#include "game/combat/projectiles/bullet.h"
#include "game/combat/projectiles/bullet_behavior/behavior_list.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/resources/resource_service.h"
#include "engine/core/render/render_command.h"
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace
{
using elysia::core::Rect;
using elysia::core::Vector2;
using namespace game::collision::categories;

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

template<class T> std::vector<T*> objects(elysia::scene::Scene& scene)
{
    std::vector<T*> result;
    static_cast<const elysia::object_query::IGameObjectQueryRuntime&>(scene)
        .visit_game_objects(elysia::core::DepthLayerMask::all(), [&](auto& object) {
            if (auto* typed = dynamic_cast<T*>(&object); typed && !typed->is_destroyed())
                result.push_back(typed);
            return true;
        });
    return result;
}

void texture_and_geometry()
{
    // Decode the actual PNGs and check that Gate selects the registered resource.
    const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface{
        SDL_CreateSurface(128, 128, SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface};
    check(surface != nullptr, "create gate texture test surface");
    const std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer{
        SDL_CreateSoftwareRenderer(surface.get()), SDL_DestroyRenderer};
    check(renderer != nullptr, "create gate texture test renderer");
    auto* resources = elysia::resources::ResourceManager::instance();
    struct ClearResources { ~ClearResources() { elysia::resources::ResourceManager::instance()->clear(); } } cleanup;
    const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
    for (const auto* key : {"gate_up", "gate_down"})
    {
        const auto path = root / "assets/textures/tile" / (std::string{key} + ".png");
        elysia::resources::TexturePtr texture{IMG_LoadTexture(renderer.get(), path.string().c_str())};
        check(texture != nullptr, "load the actual stone gate texture");
        check(resources->store_texture(key, std::move(texture)).has_value(), "register gate texture key");
    }

    Gate gate({64, 64});
    const Rect ground{64, 64, 32, 32};
    check(!gate.is_raised() && gate.world_rect() == ground && gate.render_rect() == ground,
          "lowered gate is one ground tile");
    std::vector<elysia::core::RenderCommand> commands;
    gate.submit_render_commands(commands);
    check(commands.size() == 1 && commands[0].texture == ELYSIA_RESOURCES->find_texture("gate_down"),
          "lowered gate renders gate_down");
    gate.set_raised(true);
    check(gate.is_raised() && gate.render_rect() == ground, "raising enables collision before the visual moves");
    gate.update(Gate::kAnimationDuration * 0.25);
    commands.clear();
    gate.submit_render_commands(commands);
    check(commands.size() == 2 && commands[0].texture == ELYSIA_RESOURCES->find_texture("gate_down")
        && commands[1].texture == ELYSIA_RESOURCES->find_texture("gate_up")
        && commands[1].command_rect == Rect{64, 80, 32, 16}
        && commands[1].use_src_rect && commands[1].src_rect == Rect{0, 0, 32, 16},
          "rising sprite translates upward and crops underground pixels without stretching");
    gate.set_raised(true); // RoomScene repeats the target every update.
    gate.update(Gate::kAnimationDuration * 0.75);
    commands.clear();
    gate.submit_render_commands(commands);
    check(commands.size() == 2 && commands[1].command_rect == Rect{64, 32, 32, 64}
        && commands[1].src_rect == Rect{0, 0, 32, 64}, "repeated commands do not restart a complete rise");
    const auto& collider = gate.collider_definitions().front();
    check(gate.world_rect() == ground && collider.enabled && collider.filter.category == World
        && std::get<elysia::physics::BoxShape>(collider.shape).local_bounds() == Rect{0, 0, 32, 32},
          "raised collision only occupies the ground tile and belongs to World");

    gate.set_raised(false);
    check(!gate.is_raised() && gate.render_rect() == Rect{64, 32, 32, 64},
          "lowering disables collision immediately without snapping the visual");
    gate.update(Gate::kAnimationDuration * 0.25);
    commands.clear();
    gate.submit_render_commands(commands);
    check(commands.back().command_rect == Rect{64, 48, 32, 48}, "lowering translates downward at the same speed");
    gate.set_raised(true);
    check(gate.render_rect() == Rect{64, 48, 32, 48}, "reversing preserves the current visual position");
    gate.update(Gate::kAnimationDuration * 0.25);
    check(gate.render_rect() == Rect{64, 32, 32, 64}, "reverse motion completes from the current progress");
    gate.set_raised(false);
    gate.update(Gate::kAnimationDuration * 0.5);
    gate.set_raised(false);
    gate.update(Gate::kAnimationDuration * 0.5);
    commands.clear();
    gate.submit_render_commands(commands);
    check(commands.size() == 1 && commands[0].command_rect == ground,
          "fully lowered gate leaves only the ground texture");
    gate.set_raised(true);
    gate.update(Gate::kAnimationDuration * 0.25);
    gate.reset();
    check(!gate.is_raised() && gate.render_rect() == ground, "reset lowers a gate");
    gate.update(Gate::kAnimationDuration);
    check(gate.render_rect() == ground, "reset clears an in-progress animation");
}

class PhysicsTestScene : public elysia::scene::Scene
{
public:
    PhysicsTestScene() : Scene({.fixed_step = elysia::scene::FixedStepConfig{},
                                .physics = elysia::physics::PhysicsWorldConfig{}}) {}
    using Scene::physics_world;
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
};

class MovingBody : public elysia::core::GameObject, public elysia::physics::PhysicsParticipant
{
    elysia::physics::Collider _body;
public:
    explicit MovingBody(CollisionBits category) : GameObject(elysia::core::DepthLayer::Character)
    {
        set_world_rect({20, 100, 16, 16});
        _body.shape = elysia::physics::BoxShape::from_rect({0, 0, 16, 16});
        _body.filter.category = category;
        _body.filter.mask = World;
    }
    elysia::physics::BodyDefinition body_definition() const override
    {
        elysia::physics::BodyDefinition body;
        body.type = elysia::physics::BodyType::Dynamic;
        body.gravity_scale = 0;
        return body;
    }
    std::span<const elysia::physics::Collider> collider_definitions() const override { return {&_body, 1}; }
};

void blocking_and_toggle()
{
    for (const auto category : {Player, Enemy})
    {
        test::SceneHarness<PhysicsTestScene> host;
        auto& scene = host.scene();
        auto* gate = scene.create_and_add_object<Gate>(Vector2{100, 96}, true);
        auto* mover = scene.create_and_add_object<MovingBody>(category);
        const auto body_id = gate->physics_handle();
        const auto collider_id = gate->physics_collider(0);
        mover->set_velocity({200, 0});
        for (int i = 0; i < 40; ++i) host.update(1.0 / 60);
        check(mover->world_rect().right() <= gate->world_rect().left() + 0.1f,
              "raised world gate blocks both player and enemy bodies");
        gate->set_raised(false);
        mover->set_velocity({200, 0});
        for (int i = 0; i < 40; ++i) host.update(1.0 / 60);
        check(mover->world_rect().left() > gate->world_rect().right(), "lowering enables passage immediately");
        gate->set_raised(true);
        check(gate->physics_handle() == body_id && gate->physics_collider(0) == collider_id
            && scene.physics_world().registered_object_count() == 2, "toggles keep one registered body and collider");
        scene.physics_world().teleport_object(mover->physics_handle(), {20, 100});
        mover->set_velocity({200, 0});
        for (int i = 0; i < 40; ++i) host.update(1.0 / 60);
        check(mover->world_rect().right() <= 100.1f, "raising again restores blocking");
    }
}

void animation_pause()
{
    test::SceneHarness<PhysicsTestScene> host;
    auto* gate = host.scene().create_and_add_object<Gate>(Vector2{64, 64}, true);
    check(gate->render_rect() == Rect{64, 32, 32, 64}, "a raised constructor starts at the raised visual endpoint");
    gate->set_raised(false);
    host.update(Gate::kAnimationDuration * 0.25);
    const auto middle = gate->render_rect();
    host.scene().pause();
    host.update(1);
    check(gate->render_rect() == middle, "scene pause freezes gate animation");
    host.scene().resume();
    host.update(Gate::kAnimationDuration * 0.75);
    check(gate->render_rect() == gate->world_rect(), "resuming completes the remaining gate animation");
}

void projectile_impacts()
{
    for (int scenario = 0; scenario < 3; ++scenario)
    {
        test::SceneHarness<PhysicsTestScene> host;
        auto& scene = host.scene();
        scene.create_and_add_object<Gate>(Vector2{100, 96}, scenario != 1);
        Bullet_Attributes attributes;
        attributes.start_position = {30, 112};
        attributes.starting_velocity = {300, 0};
        if (scenario == 2)
            attributes.behavior_appenders.push_back([](BulletBehaviorSet& set) {
                set.add(std::make_unique<BounceBehavior>(1));
            });
        auto* bullet = scene.create_and_add_object<Bullet>(attributes);
        bullet->initialize_fire();
        for (int i = 0; i < 30; ++i) host.update(1.0 / 60);
        const auto bullets = objects<Bullet>(scene);
        if (scenario == 0)
            check(bullets.empty(), "ordinary bullets are destroyed by raised gate world impacts");
        else if (scenario == 1)
            check(bullets.size() == 1 && bullets.front()->position().x > 132,
                  "bullets pass through lowered gates");
        else
            check(bullets.size() == 1 && bullets.front()->projectile_velocity().x < 0,
                  "existing world bounce behavior also works against gates");
    }
}

class LockedRoom : public FixedRoomScene
{
public:
    bool locked = true;
    using RoomScene::physics_world;
protected:
    bool can_leave_room() const override { return !locked; }
};

void room_gates()
{
    RoomScenePayload payload{.character = *CharacterManager::instance()->create_context("default_player")};
    auto& config = payload.room.map_config;
    config.numSubRooms = 0;
    config.baseRoomSize = {15, 15};
    config.corridors.directions = RoomDirection::All;
    payload.room.grid_size = config.baseRoomSize + config.corridors.padding() * 2;
    test::SceneHarness<LockedRoom> host(payload);
    auto& scene = host.scene();
    for (int visit = 0; visit < 3; ++visit)
    {
        if (visit)
        {
            scene.locked = true;
            host.enter(payload, visit == 1 ? elysia::scene::SceneReloadMode::Reuse
                                           : elysia::scene::SceneReloadMode::Reset);
        }
        const auto gates = objects<Gate>(scene);
        check(gates.size() == 20 && objects<Portal>(scene).size() == 1,
              "four five-tile gate rows coexist with the central portal");
        const auto* map = objects<DungeonRoom>(scene).front();
        for (const auto& corridor : map->corridors())
        {
            const auto strip = corridor.gate_strip(map->tile_size());
            int count = 0;
            for (const auto* gate : gates)
                if (strip.contains(gate->world_rect())) ++count;
            check(count == 5 && game::walkable_bounds(*map, strip),
                  "every gate row covers a full walkable mouth at the original room wall");
        }
        check(std::ranges::all_of(gates, &Gate::is_raised), "locked room starts with raised gates");
        scene.locked = false;
        host.update(0);
        check(std::ranges::none_of(gates, &Gate::is_raised), "room leaving condition lowers all gates");
        scene.locked = true;
        host.update(0);
        check(std::ranges::all_of(gates, &Gate::is_raised), "room leaving condition raises all gates");
        host.leave();
        check(objects<Gate>(scene).empty() && scene.physics_world().registered_object_count() == 0,
              "exit removes gate observers and physics registrations before reuse");
    }
}
}

void gate_tests()
{
    texture_and_geometry();
    blocking_and_toggle();
    animation_pause();
    projectile_impacts();
    room_gates();
}
