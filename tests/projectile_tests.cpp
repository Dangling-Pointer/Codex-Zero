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

class TestScene final : public elysia::scene::Scene
{
public:
    using Scene::physics_world;
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void reset() override {}
};

class RecordingGameScene final : public GameScene
{
public:
    std::size_t created_bullets = 0;
protected:
    void on_scene_object_registered(elysia::core::SceneObject& object) override
    {
        GameScene::on_scene_object_registered(object);
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
    TestScene scene;
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
    TestScene scene;
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
    const auto& player_filter = player->collider_definitions()[0].filter;
    const auto& enemy_filter = enemy->collider_definitions()[0].filter;
    check(player_projectile_filter.category == game::collision::categories::PlayerAttack
        && enemy_projectile_filter.category == game::collision::categories::EnemyAttack,
        "projectile collision categories");
    check((player_projectile_filter.mask & game::collision::categories::Enemy) != 0
        && (player_projectile_filter.mask & game::collision::categories::Player) == 0
        && (player_projectile_filter.mask & game::collision::categories::PlayerAttack) == 0,
        "player projectile target mask");
    check((enemy_projectile_filter.mask & game::collision::categories::Player) != 0
        && (enemy_projectile_filter.mask & game::collision::categories::Enemy) == 0
        && (enemy_projectile_filter.mask & game::collision::categories::EnemyAttack) == 0,
        "enemy projectile target mask");
    check(neutral_projectile_filter.category == game::collision::categories::NeutralAttack
        && (neutral_projectile_filter.mask & game::collision::categories::Player) != 0
        && (neutral_projectile_filter.mask & game::collision::categories::Enemy) != 0
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
    check(f.scene.physics_world().registered_collider_count() == 5, "one collider per bullet");
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
    f.scene.on_update(0); // Actually releases source storage before the queue sees it.
    f.manager.update(1);
    check(f.manager.pending_count() == 0, "released source safely canceled");
}

void validation_and_service()
{
    auto* service = ProjectileService::instance();
    check(!service->request_fire({}), "no active manager");
    Fixture f;
    check(service->bind_manager(f.manager), "bind service");
    Fixture other;
    check(!service->bind_manager(other.manager), "reject competing manager");
    check(!service->unbind_manager(other.manager), "wrong owner cannot detach");
    check(!service->request_fire(f.request({})), "reject empty shots");
    check(!service->request_fire({elysia::gameplay::collision::InvalidActorId, {}, {shot()}}), "reject invalid source");
    check(!service->request_fire(other.request({shot()})), "reject other scene source");
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
    f.scene.on_update(0);
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

void game_scene_input_and_pause()
{
    RecordingGameScene scene;
    scene.on_enter({});
    elysia::input::RawInputFrame input{};
    elysia::input::RawInputEvent event{};
    event.control = elysia::input::RawInputControl::KeyF;
    event.type = elysia::input::RawInputEventType::ControlReleased;
    scene.on_input(input, {event});
    scene.on_update(0.01);
    check(scene.created_bullets == 0, "key release does not fire");
    event.type = elysia::input::RawInputEventType::ControlPressed;
    scene.on_input(input, {event});
    scene.pause();
    scene.on_input(input, {event});
    scene.on_update(1);
    check(scene.created_bullets == 0, "pause freezes queued shots");
    scene.resume();
    scene.on_update(0.01);
    check(scene.created_bullets == 0, "pause did not advance delay");
    scene.on_update(0.20);
    Wand wand;
    auto shots = wand.attack({1, 0});
    std::size_t first = 0;
    for (const auto& s : shots) if (s.spawn_delay_sec <= 0.21f) ++first;
    // Count registrations: existing Block collision rules can destroy overlapping
    // scatter bullets during the same update, independently of scheduling.
    check(scene.created_bullets == first, "single press batch and paused input rejected");
    scene.on_exit();
    check(objects<Bullet>(scene).empty(), "exit destroys projectiles");
    scene.reset();
    scene.on_enter({});
    scene.on_update(0.01);
    check(objects<Bullet>(scene).empty(), "reentry has no residual shots");
    check(scene.created_bullets == first, "reentry did not emit old queued shots");
    scene.on_exit();
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
    f.scene.on_update(1.0 / 60);
    check(bullet->center().x > x, "spawned bullet moves");
    bool bounced = false;
    for (int i = 0; i < 30; ++i)
    {
        f.scene.on_update(1.0 / 60);
        auto live = objects<Bullet>(f.scene);
        check(live.size() == 1, "bullet survives its first wall bounce");
        if (live.front()->projectile_velocity().x < 0) { bounced = true; break; }
    }
    check(bounced, "wall reflection preserved");
}
}

int main()
{
    try
    {
        projectile_collision_filter();
        timing_and_order();
        moving_and_destroyed_sources();
        validation_and_service();
        nested_wand();
        game_scene_input_and_pause();
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
