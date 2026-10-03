#include <cstdlib>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#define NOMINMAX
#include <Windows.h>
#undef near
#endif
#include "game/scene/game_scene.h"
#include "game/characters/player_character.h"
#include "game/characters/enemy.h"
#include "game/combat/projectiles/bullet.h"
#include "game/combat/projectiles/bullet_behavior/behavior_list.h"
#include "game/combat/projectile_service.h"
#include "engine/core/render/render_command.h"
#include "engine/core/render/colors.h"
#include <limits>
#include <stdexcept>
#include <iostream>
#include <cmath>

void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
class TestScene : public GameScene
{
public:
    using GameScene::physics_world;
    using GameScene::collision_runtime;
};
class CountingEnemy : public Enemy
{
public:
    using Enemy::Enemy;
    int deaths = 0;
    void on_death() noexcept override { ++deaths; }
};
class MultiHurt : public Character
{
public:
    MultiHurt() : Character({100,100}, {30,30}, {0,0,30,30}, 200,
        elysia::gameplay::collision::teams::Enemy, 100, {{0,0,30,30},{0,0,25,25}}) {}
    void submit_render_commands(std::vector<elysia::core::RenderCommand>&) const override {}
};
Bullet* fire(TestScene& scene, Character& source, elysia::core::Vector2 at,
             float damage = 20, int pierce = 0,
             game::collision::categories::CollisionBits category = game::collision::categories::PlayerAttack)
{
    Bullet_Attributes a;
    a.start_position = at;
    a.damage = damage;
    a.collision_category = category;
    if (pierce) a.behavior_appenders.push_back([pierce](auto& b) {
        b.add(std::make_unique<PierceBehavior>(pierce));
    });
    auto bullet = std::make_unique<Bullet>(a);
    bullet->set_instigator(source.actor_id());
    return scene.add_object(std::move(bullet));
}
void step(TestScene& scene) { scene.on_update(1.0/60); }
void health()
{
    CountingEnemy enemy({0,0});
    check(!enemy.receive_attack({0,0,-1}).accepted, "negative rejected");
    check(!enemy.receive_attack({0,0,std::numeric_limits<float>::infinity()}).accepted, "infinity rejected");
    check(!enemy.receive_attack({0,0,std::numeric_limits<float>::quiet_NaN()}).accepted, "nan rejected");
    check(enemy.receive_attack({0,0,0}).health_lost == 0, "zero");
    check(enemy.receive_attack({0,0,30}).health_lost == 30 && enemy.health() == 70, "damage");
    auto killed = enemy.receive_attack({0,0,200});
    check(killed.killed && killed.health_lost == 70 && enemy.deaths == 1, "overkill");
    check(enemy.is_dead() && !enemy.is_destroyed() && enemy.is_visible(), "corpse retained");
    for (const auto& collider : enemy.collider_definitions()) check(!collider.enabled, "dead collider disabled");
    std::vector<elysia::core::RenderCommand> commands;
    enemy.submit_render_commands(commands);
    check(commands.size()==1 && commands.front().color == elysia::core::colors::gray_500, "death rendered gray");
    check(!enemy.receive_attack({0,0,1}).accepted && enemy.deaths == 1, "one death");
    Enemy destroyed({0,0}); destroyed.destroy();
    check(!destroyed.receive_attack({0,0,1}).accepted, "destroyed rejected");
}
void routing()
{
    TestScene scene;
    auto* source = scene.create_and_add_object<PlayerCharacter>(elysia::core::Vector2{500,500});
    auto* enemy = scene.create_and_add_object<MultiHurt>();
    auto* bullet = fire(scene,*source,{110,110},20,5);
    step(scene);
    check(enemy->health()==80, "multi hurt deduplicated");
    step(scene); check(enemy->health()==80, "persistent overlap once");
    scene.physics_world().teleport_object(bullet->physics_handle(),{300,300}); step(scene);
    scene.physics_world().teleport_object(bullet->physics_handle(),{110,110}); step(scene);
    check(enemy->health()==80, "reentry once");
    fire(scene,*source,{110,110}); step(scene);
    check(enemy->health()==60, "separate bullet");
    source->destroy(); step(scene);
    auto* other = scene.create_and_add_object<::Enemy>(elysia::core::Vector2{300,300});
    scene.physics_world().teleport_object(bullet->physics_handle(),{310,310}); step(scene);
    check(other->health()==80, "source removal keeps projectile");
    other->receive_attack({0,0,100}); step(scene);
    check(other->velocity().is_zero(), "death stops movement");
    auto* passer = scene.create_and_add_object<PlayerCharacter>(elysia::core::Vector2{305,305});
    const auto position = passer->position(); step(scene);
    check(passer->position().distance_squared_to(position)<0.001f, "dead body does not push player");
}
void teams_and_zero()
{
    using namespace game::collision::categories;
    TestScene scene;
    auto* player = scene.create_and_add_object<PlayerCharacter>(elysia::core::Vector2{100,100});
    auto* enemy = scene.create_and_add_object<::Enemy>(elysia::core::Vector2{300,300});
    auto* zero = fire(scene,*player,{310,310},0,1);
    step(scene); check(enemy->health()==100 && !zero->is_destroyed(), "zero keeps projectile");
    fire(scene,*player,{110,110}); step(scene); check(player->health()==100,"friendly ignored");
    fire(scene,*enemy,{110,110},20,0,EnemyAttack); step(scene); check(player->health()==80,"enemy attack");
    fire(scene,*player,{110,110},20,0,NeutralAttack); step(scene); check(player->health()==60,"neutral player");
    fire(scene,*player,{310,310},20,0,NeutralAttack); step(scene); check(enemy->health()==80,"neutral enemy");
}
void growth_pause_and_queue()
{
    TestScene scene;
    auto* source = scene.create_and_add_object<PlayerCharacter>(elysia::core::Vector2{500,500});
    auto* enemy = scene.create_and_add_object<::Enemy>(elysia::core::Vector2{100,100});
    Bullet_Attributes a;
    a.start_position = {110,110}; a.damage = 10;
    a.behavior_appenders.push_back([](auto& set) { set.add(std::make_unique<GrowthBehavior>(60)); });
    auto owned = std::make_unique<Bullet>(a); owned->set_instigator(source->actor_id());
    scene.add_object(std::move(owned));
    scene.pause(); scene.on_update(1); check(enemy->health()==100, "pause prevents hits");
    scene.resume(); step(scene); check(std::abs(enemy->health()-89)<0.001f, "growth at impact");
    ProjectileManager manager; manager.bind_scene(scene,scene.physics_world());
    ShotDescriptor delayed; delayed.spawn_delay_sec=1;
    check(manager.enqueue_fire_request({source->actor_id(),source->physics_handle(),{delayed}}), "queue before death");
    auto* surviving = fire(scene,*source,{300,300},15,2);
    source->receive_attack({0,0,100});
    manager.update(0); check(manager.pending_count()==0,"dead source cancels queued shots");
    check(!manager.enqueue_fire_request({source->actor_id(),source->physics_handle(),{delayed}}),"dead source cannot fire");
    scene.physics_world().teleport_object(surviving->physics_handle(),{110,110}); step(scene);
    check(enemy->health()==74,"dead source projectile still damages");
    scene.on_exit(); step(scene);
    check(scene.physics_world().registered_object_count()==0,"exit clears directly created characters and bullets");
}
void lifecycle()
{
    TestScene scene;
    for (int i=0;i<3;++i) { scene.on_enter({}); scene.on_exit(); step(scene); }
    scene.on_enter({});
    auto bad = std::make_unique<Bullet>(Bullet_Attributes{});
    auto* rejected = scene.add_object(std::move(bad));
    check(rejected->is_destroyed(), "unowned bullet registration rejected");
    step(scene);
}
int main()
{
#if defined(_MSC_VER) && defined(_DEBUG)
    _set_invalid_parameter_handler([](const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned line, uintptr_t) {
        std::wcerr << L"Invalid parameter: " << (expression ? expression : L"?") << L" in " << (function ? function : L"?") << L" " << (file ? file : L"?") << L":" << line << std::endl;
        std::_Exit(2);
    });
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
    try { std::cerr << "health\n"; health(); std::cerr << "routing\n"; routing(); std::cerr << "teams\n"; teams_and_zero(); growth_pause_and_queue(); std::cerr << "lifecycle\n"; lifecycle(); std::cout << "Combat tests passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
