#include "scene_harness.h"
#include <cstdlib>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#define NOMINMAX
#include <Windows.h>
#undef near
#endif
#include "game/combat/projectile_manager.h"
#include "game/combat/projectile_service.h"
#include "game/combat/projectiles/bullet.h"
#include "game/combat/projectiles/bullet_behavior/bullet_behavior_context.h"
#include "game/combat/projectiles/bullet_behavior/behavior_list.h"
#include "game/combat/wand/wand.h"
#include "game/combat/collision/combat_collision_categories.h"
#include "game/characters/enemy.h"
#include "game/characters/player_character.h"
#include "game/scene/game_scene.h"
#include "game/scene/combat_scene.h"
#include "game/characters/character_manager.h"
#include "game/input/game_actions.h"
#include "game/input/game_input_map.h"
#include "game/combat/player_fire.h"
#include "engine/scene/scene.h"
#include "engine/physics/physics_world.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class TestScene : public elysia::scene::Scene
{
public:
    TestScene() : Scene({.fixed_step = elysia::scene::FixedStepConfig{},
                         .physics = elysia::physics::PhysicsWorldConfig{}}) {}
    using Scene::physics_world;
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
};

template<class Base> class RecordingScene : public Base
{
public:
    std::size_t created_bullets = 0;
protected:
    void on_scene_object_registered(elysia::core::SceneObject& object) override
    {
        Base::on_scene_object_registered(object);
        if (dynamic_cast<Bullet*>(&object)) ++created_bullets;
    }
};

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

struct Fixture
{
    test::SceneHarness<TestScene> host;
    TestScene& scene = host.scene();
    ProjectileManager manager;
    PlayerCharacter* source = scene.create_and_add_object<PlayerCharacter>(elysia::core::Vector2{100, 100});
    elysia::gameplay::collision::ActorId source_actor = source->actor_id();
    Fixture() { manager.bind_scene(scene, scene.physics_world()); }

    ProjectileFireRequest request(std::vector<ShotDescriptor> shots) const
    {
        return {source_actor, source->physics_handle(), std::move(shots)};
    }
};

ShotDescriptor shot(float delay = 0)
{
    ShotDescriptor result;
    result.spawn_delay_sec = delay;
    result.spawn_offset = {100, 0};
    result.bullet_attributes.starting_velocity = {50, 0};
    return result;
}

void projectile_collision_filter()
{
    test::SceneHarness<TestScene> host;
    TestScene& scene = host.scene();
    auto* player_projectile = scene.create_and_add_object<Projectile>();
    auto* enemy_projectile = scene.create_and_add_object<Projectile>(
        elysia::core::Vector2{}, elysia::core::Vector2{1, 1}, elysia::core::Vector2{},
        game::collision::categories::EnemyAttack);
    auto* neutral_projectile = scene.create_and_add_object<Projectile>(
        elysia::core::Vector2{}, elysia::core::Vector2{1, 1}, elysia::core::Vector2{},
        game::collision::categories::NeutralAttack);
    auto* player = scene.create_and_add_object<PlayerCharacter>(elysia::core::Vector2{100, 100});
    auto* enemy = scene.create_and_add_object<Enemy>(elysia::core::Vector2{200, 100});
    const auto& player_projectile_filter = player_projectile->collider_definitions()[0].filter;
    const auto& enemy_projectile_filter = enemy_projectile->collider_definitions()[0].filter;
    const auto& neutral_projectile_filter = neutral_projectile->collider_definitions()[0].filter;
    const auto& player_filter = player->collider_definitions()[1].filter;
    const auto& enemy_filter = enemy->collider_definitions()[1].filter;
    check(player_projectile_filter.category == game::collision::categories::PlayerAttack
        && enemy_projectile_filter.category == game::collision::categories::EnemyAttack,
        "projectile collision categories");
    check((player_projectile_filter.mask & game::collision::categories::EnemyHurt) != 0
        && (player_projectile_filter.mask & game::collision::categories::Player) == 0
        && (player_projectile_filter.mask & game::collision::categories::PlayerAttack) == 0,
        "player projectile target mask");
    check((enemy_projectile_filter.mask & game::collision::categories::PlayerHurt) != 0
        && (enemy_projectile_filter.mask & game::collision::categories::Enemy) == 0
        && (enemy_projectile_filter.mask & game::collision::categories::EnemyAttack) == 0,
        "enemy projectile target mask");
    check(neutral_projectile_filter.category == game::collision::categories::NeutralAttack
        && (neutral_projectile_filter.mask & game::collision::categories::PlayerHurt) != 0
        && (neutral_projectile_filter.mask & game::collision::categories::EnemyHurt) != 0
        && (neutral_projectile_filter.mask & game::collision::categories::NeutralAttack) == 0,
        "neutral projectile target mask");
    check((player_filter.mask & game::collision::categories::EnemyAttack) != 0,
        "player ignores projectiles");
    check((player_filter.mask & game::collision::categories::NeutralAttack) != 0
        && (enemy_filter.mask & game::collision::categories::PlayerAttack) != 0
        && (enemy_filter.mask & game::collision::categories::NeutralAttack) != 0,
        "enemy accepts projectiles");
}

class RecordFire final : public BulletBehavior
{
public:
    RecordFire(std::vector<int>& records, int value) : _records(records), _value(value) {}
    void on_fire(BulletBehaviorContext&) override { _records.push_back(_value); }
private:
    std::vector<int>& _records;
    int _value;
};

void timing_and_order()
{
    std::vector<int> records;
    Fixture f;
    const auto marked = [&](float delay, int id) {
        auto s = shot(delay);
        s.bullet_attributes.behavior_appenders.push_back([&, id](BulletBehaviorSet& set) {
            set.add(std::make_unique<RecordFire>(records, id));
        });
        return s;
    };
    check(f.manager.enqueue_fire_request(f.request({marked(0.5f, 3), marked(0.25f, 1), marked(0.25f, 2)})), "queue sequence");
    check(f.manager.enqueue_fire_request(f.request({marked(-1, 0)})), "negative delay accepted");
    check(records.empty(), "submission must not spawn");
    f.manager.update(0);
    check(records == std::vector<int>{0}, "zero delay fires on next update");
    f.manager.update(0.125);
    check(records.size() == 1, "not early");
    f.manager.update(0.875);
    check(records == std::vector<int>({0, 1, 2, 3}), "due order and stable ties");
    f.manager.update(0);
    check(records.size() == 4 && f.manager.pending_count() == 0, "exactly once");
    check(f.scene.physics_world().registered_object_count() == 5, "one physics registration per bullet");
    check(f.scene.physics_world().registered_collider_count() == 6, "one collider per bullet");
}

void moving_and_destroyed_sources()
{
    Fixture f;
    check(f.manager.enqueue_fire_request(f.request({shot(0.5f)})), "moving source request");
    const elysia::core::Vector2 moved_position{300.0f, 200.0f};
    check(f.scene.physics_world().teleport_object(
        f.source->physics_handle(),
        moved_position),
        "move source body");
    f.manager.update(0.5);
    auto bullets = objects<Bullet>(f.scene);
    check(bullets.size() == 1, "one moving-source bullet");
    check(bullets[0]->center().distance_squared_to(elysia::core::Vector2{400, 200}) < 0.001f, "spawn from current position");
    check(std::abs(bullets[0]->projectile_velocity().x - 50) < 0.001f, "preserve authored velocity");
    check(f.manager.enqueue_fire_request(f.request({shot(1)})), "cancel request");
    f.source->destroy();
    f.manager.update(0);
    check(f.manager.pending_count() == 0, "cancel immediately when marked destroyed");
    check(objects<Bullet>(f.scene).size() == 1, "existing bullet survives source");

    auto* source2 = f.scene.create_and_add_object<PlayerCharacter>(elysia::core::Vector2{500, 500});
    check(f.manager.enqueue_fire_request({source2->actor_id(), source2->physics_handle(), {shot(1)}}), "deleted source request");
    source2->destroy();
    f.host.update(0); // Actually releases source storage before the queue sees it.
    f.manager.update(1);
    check(f.manager.pending_count() == 0, "released source safely canceled");
}

void validation_and_service()
{
    auto* service = ProjectileService::instance();
    check(!service->request_fire({}), "no active manager");
    Fixture f;
    check(service->bind_manager(f.manager), "bind service");
    TestScene other_scene;
    ProjectileManager other_manager;
    other_manager.bind_scene(other_scene, other_scene.physics_world());
    auto* other_source = other_scene.create_and_add_object<PlayerCharacter>(elysia::core::Vector2{100,100});
    check(!service->bind_manager(other_manager), "reject competing manager");
    check(!service->unbind_manager(other_manager), "wrong owner cannot detach");
    check(!service->request_fire(f.request({})), "reject empty shots");
    check(!service->request_fire({elysia::gameplay::collision::InvalidActorId, {}, {shot()}}), "reject invalid source");
    check(!service->request_fire(ProjectileFireRequest{other_source->actor_id(), other_source->physics_handle(), {shot()}}), "reject other scene source");
    auto invalid = shot();
    invalid.spawn_delay_sec = std::numeric_limits<float>::quiet_NaN();
    check(!service->request_fire(f.request({shot(), invalid})), "reject entire invalid request");
    check(f.manager.pending_count() == 0, "no partial enqueue");
    invalid = shot();
    invalid.bullet_attributes.starting_velocity.x = std::numeric_limits<float>::infinity();
    check(!service->request_fire(f.request({invalid})), "reject infinite velocity");
    check(service->request_fire(f.request({shot(), shot(1)})), "service accepts valid request");
    f.manager.update(std::numeric_limits<double>::quiet_NaN());
    check(objects<Bullet>(f.scene).empty(), "invalid delta ignored");
    f.manager.update(0);
    check(objects<Bullet>(f.scene).size() == 1, "service routes to manager");
    f.manager.unbind_scene();
    check(f.manager.pending_count() == 0 && objects<Bullet>(f.scene).empty(), "unbind clears live and queued bullets");
    check(!service->request_fire(f.request({shot()})), "service detached");
    f.host.update(0);
    check(f.scene.physics_world().registered_object_count() == 1, "bullet physics removed");
    f.manager.bind_scene(f.scene, f.scene.physics_world());
    check(f.manager.enqueue_fire_request(f.request({shot()})), "rebind usable");
    f.manager.update(0);
    check(objects<Bullet>(f.scene).size() == 1, "no old queue after rebind");
}

void nested_wand()
{
    Fixture f;
    Wand wand;
    auto shots = wand.attack({1, 0});
    check(!shots.empty(), "wand produces shots");
    const auto all = shots.size();
    std::size_t first = 0;
    float earliest = std::numeric_limits<float>::max(), latest = 0;
    for (const auto& s : shots) { earliest = std::min(earliest, s.spawn_delay_sec); latest = std::max(latest, s.spawn_delay_sec); }
    for (const auto& s : shots) if (s.spawn_delay_sec == earliest) ++first;
    check(latest > earliest, "test wand has nested delayed shots");
    check(f.manager.enqueue_fire_request(f.request(std::move(shots))), "enqueue wand output");
    f.manager.update(earliest);
    check(objects<Bullet>(f.scene).size() == first, "first wand batch only");
    f.manager.update(latest - earliest + 0.001);
    check(objects<Bullet>(f.scene).size() == all, "all nested shots eventually spawn");
}


// Preserve source initial and final states so edges use the same routing path as real input.
struct InputSequence
{
    elysia::input::RawInputState keyboard, mouse;
    elysia::input::InputSnapshot next(std::vector<elysia::input::RawInputEvent> events = {},
        elysia::input::InputCapture capture = elysia::input::InputCapture::None, bool focus_lost = false)
    {
        using namespace elysia::input;
        InputSnapshot snapshot;
        snapshot.sources = {{.source = InputSourceId::keyboard(), .initial_state = keyboard},
                            {.source = InputSourceId::mouse(), .initial_state = mouse}};
        for (const auto& event : events)
            apply_raw_event(event.source.is_mouse() ? mouse : keyboard, event);
        snapshot.sources[0].frame.state = keyboard;
        snapshot.sources[1].frame.state = mouse;
        snapshot.events = std::move(events);
        snapshot.capture = capture;
        snapshot.focus_lost = focus_lost;
        return snapshot;
    }
};
elysia::input::RawInputEvent button(elysia::input::RawInputControl control, bool pressed,
                                   elysia::core::Vector2 pointer = {})
{
    using namespace elysia::input;
    RawInputEvent event;
    event.control = control;
    event.type = pressed ? RawInputEventType::ControlPressed : RawInputEventType::ControlReleased;
    const bool mouse = control == RawInputControl::MouseLeft;
    event.source = mouse ? InputSourceId::mouse() : InputSourceId::keyboard();
    event.device = mouse ? InputDevice::Mouse : InputDevice::Keyboard;
    event.mouse_x = int(pointer.x); event.mouse_y = int(pointer.y);
    return event;
}
void input_map_and_aim()
{
    using namespace elysia::input;
    auto map = game::input::make_player_input_map();
    check(map.valid() && map.bindings(game::input::actions::Move).size()==4
        && map.bindings(game::input::actions::Fire).size()==2
        && !map.contains(elysia::gameplay::actions::Jump), "game map contains only intended bindings");
    InputSequence sequence;
    auto started = [&](std::vector<RawInputEvent> events) {
        auto resolved = map.resolve(sequence.next(std::move(events)));
        return std::ranges::count_if(resolved.events, [](const auto& event) {
            return event.action == game::input::actions::Fire && event.phase == ActionInputPhase::Started;
        });
    };
    check(started({button(RawInputControl::MouseLeft,true),button(RawInputControl::KeyF,true)})==1,
          "left mouse and F combine into one fire action");
    check(started({})==0, "held action has no new start");
    check(started({button(RawInputControl::MouseLeft,false)})==0, "F maintains combined held state");
    check(started({button(RawInputControl::KeyF,false)})==0, "release does not start fire");
    check(started({button(RawInputControl::KeyF,true),button(RawInputControl::KeyF,false)})==1,
          "short press between ticks retains start");
    // Resolve each retained movement binding, including the gamepad variants.
    for (auto key : {RawInputControl::KeyW, RawInputControl::KeyUp, RawInputControl::GamepadDPadUp})
    {
        auto source = key == RawInputControl::GamepadDPadUp
            ? InputSourceId{42,InputSourceKind::Gamepad} : InputSourceId::keyboard();
        InputSnapshot snapshot;
        snapshot.sources.push_back({.source=source});
        snapshot.sources[0].frame.state.set_pressed(key, true);
        auto move = game::input::make_player_input_map().resolve(snapshot).frame.axis2d(game::input::actions::Move);
        check(move.y < 0 && move.x==0, "keyboard and D-pad movement retained");
    }
    InputSnapshot stick;
    stick.sources.push_back({.source={42,InputSourceKind::Gamepad}});
    stick.sources[0].frame.state.set_axis(RawInputAxis::GamepadLeftX, 1);
    check(game::input::make_player_input_map().resolve(stick).frame.axis2d(game::input::actions::Move).x > 0,
          "left stick movement retained");

    Fixture f;
    check(ProjectileService::instance()->bind_manager(f.manager), "bind aiming test manager");
    elysia::camera::Camera camera({500,-300},{1280,720},2.5f);
    const auto center = f.source->render_rect().center();
    for (auto direction : {elysia::core::Vector2{0,-1}, elysia::core::Vector2{-1,0},
                           elysia::core::Vector2{1,1}.normalized()})
    {
        const auto pointer = camera.world_to_screen(center + direction * 200);
        check(game::player_fire_direction(*f.source, camera, pointer).nearly_equals(direction),
              "translated zoomed camera maps cursor to world direction");
    }
    check(game::player_fire_direction(*f.source,camera,{}).nearly_equals({1,0}), "missing pointer faces right");
    check(game::player_fire_direction(*f.source,camera,camera.world_to_screen(center)).nearly_equals({1,0}),
          "center pointer falls back to facing");
    elysia::gameplay::ControlCommand command;
    command.state.set(game::input::actions::Move, InputActionValueType::Axis2D, {-1,0});
    f.source->on_control_command(command, 1.0/60);
    check(game::player_fire_direction(*f.source,camera,{}).nearly_equals({-1,0}), "left facing fallback");
    command.events.push_back({game::input::actions::Fire, InputActionValueType::Button, ActionInputPhase::Started});
    command.pointer_position = camera.world_to_screen(center + elysia::core::Vector2{0,-200});
    f.source->on_control_command(command, 1.0/60);
    f.source->on_control_cancelled(InputCancelReason::FocusLost);
    check(f.source->take_fire_requests().empty(), "cancellation clears pending fire request");
    f.source->on_control_command(command, 1.0/60);
    Wand wand;
    game::fire_player_requests(*f.source,camera,wand);
    auto expected = wand.attack({0,-1});
    float first_delay = std::numeric_limits<float>::max();
    for (const auto& shot : expected) first_delay = std::min(first_delay, shot.spawn_delay_sec);
    f.manager.update(first_delay);
    auto bullets = objects<Bullet>(f.scene);
    std::vector<ShotDescriptor> first;
    for (const auto& shot : expected) if (shot.spawn_delay_sec==first_delay) first.push_back(shot);
    check(!bullets.empty() && bullets.size()==first.size(), "aimed batch reaches scheduler");
    for (std::size_t i=0;i<first.size();++i)
    {
        check(bullets[i]->projectile_velocity().nearly_equals(first[i].bullet_attributes.starting_velocity),
              "actual projectile velocity follows mouse aim and spread");
        check(bullets[i]->center().nearly_equals(center + first[i].spawn_offset),
              "actual projectile spawns relative to player center");
    }
    f.source->on_control_command(command, 1.0/60);
    f.source->receive_attack({0,0,100});
    check(f.source->take_fire_requests().empty(), "death clears pending fire request");
    f.source->on_control_command(command, 1.0/60);
    check(f.source->take_fire_requests().empty(), "dead player rejects fire");
}

template<class Base> void game_scene_input_and_pause(const elysia::scene::ScenePayload& payload = {})
{
    using namespace elysia::input;
    test::SceneHarness<RecordingScene<Base>> host(payload);
    auto& scene = host.scene();
    InputSequence input;
    constexpr double tick = 1.0/60;
    Wand wand;
    const auto shots = wand.attack({1,0});
    const auto batch = shots.size();
    double drain = 0;
    for (const auto& shot : shots) drain = std::max(drain, double(shot.spawn_delay_sec));
    drain += 0.1;
    // First input arrives before a fixed tick, then release arrives before the next.
    host.input(input.next({button(RawInputControl::MouseLeft,true,{640,100}),
                           button(RawInputControl::KeyF,true)}));
    host.input(input.next({button(RawInputControl::MouseLeft,false),button(RawInputControl::KeyF,false)}));
    host.update(tick);
    check(scene.created_bullets==0, "fixed command queues fire without immediate scene mutation");
    scene.pause(); host.update(1);
    check(scene.created_bullets==0, "pause freezes already scheduled shots");
    scene.resume(); host.update(drain);
    check(scene.created_bullets==batch, "short mouse and F press yields one complete batch");
    host.input(input.next({button(RawInputControl::KeyF,true)})); host.update(tick);
    host.update(drain); host.update(drain);
    check(scene.created_bullets==batch*2, "F fires once and held key does not repeat across fixed steps");
    host.input(input.next({button(RawInputControl::KeyF,false)})); host.update(tick);
    host.update(drain);
    check(scene.created_bullets==batch*2, "release does not fire");
    // A command cancelled before its first fixed tick must not schedule any shots.
    host.input(input.next({button(RawInputControl::KeyF,true)}));
    scene.pause(); host.update(1); scene.resume();
    host.input(input.next({button(RawInputControl::KeyF,false)})); host.update(tick); host.update(drain);
    check(scene.created_bullets==batch*2, "pause cancels unconsumed press");
    host.input(input.next({button(RawInputControl::KeyF,true)}));
    host.input(input.next({}, InputCapture::None, true));
    host.update(tick); host.update(drain);
    host.input(input.next({button(RawInputControl::KeyF,false)})); host.update(tick);
    check(scene.created_bullets==batch*2, "focus loss cancels unconsumed press");
    host.input(input.next({button(RawInputControl::MouseLeft,true,{640,100})},InputCapture::Pointer));
    host.update(tick); host.update(drain);
    host.input(input.next({button(RawInputControl::MouseLeft,false)})); host.update(tick);
    host.input(input.next({button(RawInputControl::KeyF,true)},InputCapture::Keyboard));
    host.update(tick); host.update(drain);
    host.input(input.next({button(RawInputControl::KeyF,false)})); host.update(tick);
    check(scene.created_bullets==batch*2, "captured pointer and keyboard do not fire");
    auto players = objects<PlayerCharacter>(scene);
    check(players.size()==1, "one player in either entry");
    auto start = players[0]->position();
    host.input(input.next({button(RawInputControl::KeyD,true)})); host.update(tick*2);
    check(players[0]->velocity().x>0 && players[0]->position().x>start.x, "both scenes retain mapped movement");
    host.input(input.next({button(RawInputControl::KeyD,false)})); host.update(tick);
    host.leave();
    check(objects<Bullet>(scene).empty(), "exit destroys projectiles");
    host.enter(payload);
    host.update(drain);
    check(scene.created_bullets==batch*2 && objects<Bullet>(scene).empty(), "reentry has no residual shots");
    host.input(input.next({button(RawInputControl::MouseLeft,true,{640,100})})); host.update(tick); host.update(drain);
    check(scene.created_bullets==batch*3, "mouse fire controller rebinds on reentry");
    host.input(input.next({button(RawInputControl::KeyEscape,true)}));
    check(host.manager.current_scene_key()==2, "Escape still returns to menu");
}

void movement_and_wall_collision()
{
    class Wall final : public elysia::physics::ITileCollisionWorld
    {
    public:
        elysia::core::Vector2 world_origin() const noexcept override { return {}; }
        elysia::core::Vector2 tile_size() const noexcept override { return {100, 100}; }
        int columns() const noexcept override { return 5; }
        int rows() const noexcept override { return 5; }
        elysia::physics::TileOutOfBoundsPolicy out_of_bounds_policy() const noexcept override
        { return elysia::physics::TileOutOfBoundsPolicy::Empty; }
        elysia::physics::TileCollisionCell cell_at(elysia::physics::TileCoordinate c) const noexcept override
        {
            return {.type = c.x == 3 ? elysia::physics::TileCollisionType::Block
                                   : elysia::physics::TileCollisionType::Empty};
        }
    } wall;
    Fixture f;
    check(f.scene.physics_world().set_tile_world(wall), "register test wall");
    auto s = shot();
    s.bullet_attributes.starting_velocity = {400, 0};
    s.bullet_attributes.behavior_appenders.push_back([](BulletBehaviorSet& set) {
        set.add(std::make_unique<BounceBehavior>(2));
    });
    check(f.manager.enqueue_fire_request(f.request({s})), "queue wall test");
    f.manager.update(0);
    auto* bullet = objects<Bullet>(f.scene).front();
    const auto x = bullet->center().x;
    f.host.update(1.0 / 60);
    check(bullet->center().x > x, "spawned bullet moves");
    bool bounced = false;
    for (int i = 0; i < 30; ++i)
    {
        f.host.update(1.0 / 60);
        auto live = objects<Bullet>(f.scene);
        check(live.size() == 1, "bullet survives its first wall bounce");
        if (live.front()->projectile_velocity().x < 0) { bounced = true; break; }
    }
    check(bounced, "wall reflection preserved");
}
}

int main()
{
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportHook([](int, char* message, int*) -> int {
        std::cerr << message << std::flush;
        std::_Exit(1);
    });
    _CrtSetReportHookW2(_CRT_RPTHOOK_INSTALL, [](int, wchar_t* message, int*) -> int {
        char buffer[8192]{};
        WideCharToMultiByte(CP_UTF8, 0, message, -1, buffer, sizeof(buffer), nullptr, nullptr);
        std::cerr << buffer << std::flush;
        std::_Exit(1);
    });
#endif
    try
    {
        projectile_collision_filter();
        timing_and_order();
        moving_and_destroyed_sources();
        validation_and_service();
        nested_wand();
        input_map_and_aim();
        game_scene_input_and_pause<GameScene>();
        game_scene_input_and_pause<CombatScene>(RoomScenePayload{
            *CharacterManager::instance()->create_context("default_player"), {}});
        movement_and_wall_collision();
        {
            Fixture f;
            check(ProjectileService::instance()->bind_manager(f.manager), "bind before destructor");
        }
        check(!ProjectileService::instance()->request_fire({}), "destruction detaches service");
        std::cout << "Projectile scheduling tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
