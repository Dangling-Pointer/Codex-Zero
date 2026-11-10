#include "scene_harness.h"
#include "game/scene/combat_scene.h"
#include "game/combat/projectiles/bullet.h"
#include "game/characters/character_manager.h"
#include "game/map/dungeon_room.h"
#include <limits>
#include "game/scene/gameplay_setup.h"
#include "game/enemy_generation/enemy_generator.h"
#include "game/characters/player_character.h"
#include "game/characters/enemy.h"
#include "game/combat/collision/combat_collision_categories.h"
#include "game/scene/game_scene.h"
#include "engine/gameplay/input/gameplay_input_map.h"
#include "engine/core/render/render_command.h"
#include "engine/core/render/colors.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <cstdlib>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#define NOMINMAX
#include <Windows.h>
#undef near
#endif

namespace
{
static_assert(std::is_abstract_v<Character>);
static_assert(std::is_base_of_v<Character, PlayerCharacter>);
static_assert(std::is_base_of_v<Character, Enemy>);

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
bool near(float a, float b) { return std::abs(a - b) < 0.001f; }

class TestScene : public elysia::gameplay::GameplayScene
{
public:
    TestScene() : GameplayScene({.physics = elysia::physics::PhysicsWorldConfig{}}) {}
    using Scene::physics_world;
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
};

void definitions()
{
    PlayerCharacter player({100, 200});
    Enemy enemy({300, 400});
    check(player.team() == elysia::gameplay::collision::teams::Player, "player team");
    check(enemy.team() == elysia::gameplay::collision::teams::Enemy, "enemy team");
    check(player.actor_id() != elysia::gameplay::collision::InvalidActorId
        && enemy.actor_id() != elysia::gameplay::collision::InvalidActorId
        && player.actor_id() != enemy.actor_id(), "character actor ids");
    check(near(player.move_speed(), 200) && near(enemy.move_speed(), 200), "move speeds");
    check(player.size() == elysia::core::Vector2{32, 32}, "player size retained");
    check(enemy.size() == elysia::core::Vector2{30, 30}, "enemy size retained");
    const auto p = std::get<elysia::physics::BoxShape>(player.collider_definitions()[0].shape).local_bounds();
    check(near(p.left(), 5.6f) && near(p.top(), 19.84f)
        && near(p.width(), 20.8f) && near(p.height(), 12.16f), "player foot collider retained");
    const auto e = std::get<elysia::physics::BoxShape>(enemy.collider_definitions()[0].shape).local_bounds();
    check(e == elysia::core::Rect{0, 0, 30, 30}, "enemy collider aligned with visual");
    for (Character* character : {static_cast<Character*>(&player), static_cast<Character*>(&enemy)})
    {
        check(character->collider_definitions().size() == 2, "single collider definition");
        const auto& collider = character->collider_definitions()[0];
        check(collider.response == elysia::physics::CollisionResponse::Block
            && collider.material.friction == 0 && collider.material.restitution == 0, "common material");
        const auto expected_projectile = character->team() == elysia::gameplay::collision::teams::Enemy
            ? game::collision::categories::PlayerAttack
            : game::collision::categories::EnemyAttack;
        const auto other_projectile = character->team() == elysia::gameplay::collision::teams::Enemy
            ? game::collision::categories::EnemyAttack
            : game::collision::categories::PlayerAttack;
        check((character->collider_definitions()[1].filter.mask & expected_projectile) != 0
            && (character->collider_definitions()[1].filter.mask & other_projectile) == 0
            && (character->collider_definitions()[1].filter.mask & game::collision::categories::NeutralAttack) != 0,
            "projectile target filtering");
        const auto body = character->body_definition();
        check(body.type == elysia::physics::BodyType::Dynamic && body.gravity_scale == 0
            && body.fixed_rotation && !body.enable_sleep && body.linear_damping == 0, "common body");
    }
    std::vector<elysia::core::RenderCommand> commands;
    player.submit_render_commands(commands);
    check(commands.size() == 2, "player body and direction marker");
    check(commands[0].command_rect == player.render_rect(), "player render bounds");
    commands.clear();
    enemy.submit_render_commands(commands);
    check(commands.size() == 2 && commands[0].command_rect == enemy.render_rect(), "enemy render bounds");
    check(player.collider_definitions()[0].filter.category
            == game::collision::categories::Player
        && enemy.collider_definitions()[0].filter.category
            == game::collision::categories::Enemy,
        "character collision categories");
}

void movement()
{
    test::SceneHarness<TestScene> host;
    TestScene& scene = host.scene();
    auto* player = scene.create_and_add_object<PlayerCharacter>(elysia::core::Vector2{100, 100});
    (void)game::bind_player(scene.control_context(), *player);
    auto* enemy = scene.create_and_add_object<Enemy>(elysia::core::Vector2{1000, 1000});
    check(scene.physics_world().registered_object_count() == 2
        && scene.physics_world().registered_collider_count() == 4, "one registration per character");
    auto input = [&](std::initializer_list<elysia::input::RawInputControl> keys) {
        elysia::input::RawInputFrame frame;
        for (auto key : keys) frame.state.set_pressed(key, true);
        host.input(frame, {});
        host.update(1.0 / 60.0);
    };
    using Key = elysia::input::RawInputControl;
    input({Key::KeyD});
    check(near(player->velocity().x, 200) && near(player->velocity().y, 0), "horizontal movement");
    check(player->facing() == Character::Facing::Right, "right facing");
    input({Key::KeyD, Key::KeyS});
    check(near(player->velocity().length(), 200) && player->velocity().y > 0, "diagonal normalized");
    input({Key::KeyA});
    check(near(player->velocity().x, -200) && player->facing() == Character::Facing::Left, "left movement and facing");
    input({Key::KeyW});
    check(near(player->velocity().y, -200) && player->facing() == Character::Facing::Left, "vertical preserves facing");
    input({});
    check(player->velocity().is_zero() && player->facing() == Character::Facing::Left, "release stops and preserves facing");
    check(enemy->velocity().is_zero() && enemy->position() == elysia::core::Vector2{1000, 1000}, "enemy has no autonomous movement");
    input({Key::KeyD});
    scene.pause();
    check(player->velocity().is_zero(), "pause cancels movement immediately");
    host.update(1);
    scene.resume();
    input({});
    check(player->velocity().is_zero(), "resume does not replay stale movement");
    input({Key::KeyD});
    host.input(elysia::input::InputSnapshot{.focus_lost = true});
    check(player->velocity().is_zero(), "focus loss cancels movement immediately");
    input({});
    player->destroy();
    host.update(0);
    auto* replacement = scene.create_and_add_object<PlayerCharacter>(elysia::core::Vector2{100,100});
    (void)game::bind_player(scene.control_context(), *replacement);
    input({Key::KeyA});
    check(replacement->velocity().x < 0, "removed target releases primary player binding");

}

template<class T> std::size_t count(elysia::scene::Scene& scene)
{
    std::size_t result = 0;
    static_cast<const elysia::object_query::IGameObjectQueryRuntime&>(scene)
        .visit_game_objects(elysia::core::DepthLayerMask::all(), [&](auto& object) {
            if (!object.is_destroyed() && dynamic_cast<T*>(&object)) ++result;
            return true;
        });
    return result;
}

template<class T> void scene_lifecycle()
{
    test::SceneHarness<T> host;
    auto& scene = host.scene();
    for (int i = 0; i < 3; ++i)
    {
        std::cerr << "lifecycle " << i << " enter\n";
        if (i > 0) host.enter({}, elysia::scene::SceneReloadMode::Reset);
        std::cerr << "entered\n";
        const auto enemies = count<Enemy>(scene);
        check(enemies > 0 && count<PlayerCharacter>(scene) == 1
            && count<Character>(scene) == enemies + 1, "generated enemies and one player");
        float spent = 0;
        static_cast<const elysia::object_query::IGameObjectQueryRuntime&>(scene)
            .visit_game_objects(elysia::core::DepthLayerMask::all(), [&](auto& object) {
                if (auto* enemy = dynamic_cast<Enemy*>(&object); enemy && !enemy->is_destroyed())
                    spent += 1 + static_cast<int>(enemy->archetype());
                return true;
            });
        check(near(spent, 25), "enemy purchases spend configured budget");
        host.leave();
        std::cerr << "exited\n";
        check(count<Character>(scene) == 0, "exit/reset clears characters");
        host.update(0); // Release destroyed objects; repeated cleanup must remain safe.
        std::cerr << "updated\n";
    }
}
void context_and_room()
{
    auto context = *CharacterManager::instance()->create_context("default_player");
    context.health = 37;
    context.max_health = 150;
    context.move_speed = 123;
    PlayerCharacter original({1,2}, context);
    original.receive_attack({0,0,7});
    auto saved = original.capture_context();
    PlayerCharacter restored({3,4}, saved);
    check(restored.health() == 30 && restored.max_health() == 150
        && restored.move_speed() == 123 && saved.character_id == context.character_id,
        "character context round trip");
    auto dead = saved; dead.health = 0;
    PlayerCharacter corpse({}, dead);
    check(corpse.is_dead(), "zero health restores death");
    for (const auto& collider : corpse.collider_definitions())
        check(!collider.enabled, "restored corpse colliders disabled");
    for (int i=0; i<7; ++i)
    {
        auto bad = saved;
        if (i==0) bad.character_id = "missing";
        if (i==1) bad.health = -1;
        if (i==2) bad.health = bad.max_health + 1;
        if (i==3) bad.max_health = 0;
        if (i==4) bad.move_speed = -1;
        if (i==5) bad.health = std::numeric_limits<float>::quiet_NaN();
        if (i==6) bad.move_speed = std::numeric_limits<float>::infinity();
        bool rejected = false;
        try { PlayerCharacter invalid({}, bad); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "invalid context rejected");
    }
    const RoomScenePayload payload{saved, {}};
    {
        test::SceneHarness<CombatScene> host(payload);
        auto& scene = host.scene();
        for (auto mode : {elysia::scene::SceneReloadMode::Reuse, elysia::scene::SceneReloadMode::Reset})
        {
            check(scene.capture_character_context()->health == 30, "room restores player state");
            check(count<PlayerCharacter>(scene) == 1 && count<Enemy>(scene) > 0, "combat room content");
            const DungeonRoom* map = nullptr;
            PlayerCharacter* player = nullptr;
            static_cast<const elysia::object_query::IGameObjectQueryRuntime&>(scene)
                .visit_game_objects(elysia::core::DepthLayerMask::all(), [&](auto& object) {
                    if (auto* value = dynamic_cast<DungeonRoom*>(&object)) map = value;
                    if (auto* value = dynamic_cast<PlayerCharacter*>(&object)) player = value;
                    return true;
                });
            check(map && player, "room owns map and player");
            auto at = player->position() - map->position();
            check(map->tile_map().get(int(at.x/32), int(at.y/32)).type == TileType::Floor,
                  "player spawns on walkable tile");
            elysia::input::RawInputFrame frame;
            frame.state.set_pressed(elysia::input::RawInputControl::KeyD, true);
            host.input(frame, {}); host.update(1.0/60);
            check(player->velocity().x > 0, "room player controller bound");
            check(scene.camera().center().distance_squared_to(player->render_rect().center()) < 0.01f,
                  "scene camera follows interpolated player");
            Bullet_Attributes attributes;
            attributes.start_position = player->center();
            auto bullet = std::make_unique<Bullet>(attributes);
            bullet->set_instigator(player->actor_id());
            auto* live = scene.add_object(std::move(bullet));
            host.update(0); // Register the physics listener before base-class retirement.
            static_cast<elysia::core::GameObject*>(live)->destroy();
            host.update(0);
            host.update(1.0/60); // Must not dispatch a stale projectile listener.
            host.leave();
            check(!scene.capture_character_context() && count<Character>(scene)==0, "room exit clears state");
            host.enter(payload, mode);
        }
        elysia::input::RawInputEvent escape;
        escape.type = elysia::input::RawInputEventType::ControlPressed;
        escape.control = elysia::input::RawInputControl::KeyEscape;
        // The harness uses key 2 for its empty destination, matching MainMenu.
        host.input({}, {escape});
        check(host.manager.current_scene_key() == 2, "combat room Escape returns to menu");
    }
    for (int i=0; i<3; ++i)
    {
        auto bad = payload;
        if (i==0) bad.room.grid_size.x = 0;
        if (i==1) bad.character.health = -1;
        bool rejected = false;
        try {
            test::SceneHarness<CombatScene> host(i==2 ? elysia::scene::ScenePayload{} : elysia::scene::ScenePayload{bad});
        } catch (const std::exception&) { rejected = true; }
        check(rejected, "invalid room payload fails through scene boundary");
    }
}

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
    try
    {
        context_and_room();
        definitions();
        movement();
        scene_lifecycle<GameScene>();
        std::cout << "Character tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
