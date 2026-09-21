#include "game/characters/player_character.h"
#include "game/characters/enemy.h"
#include "game/scene/game_scene.h"
#include "game/scene/room_scene.h"
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

class TestScene final : public elysia::gameplay::GameplayScene
{
public:
    using Scene::physics_world;
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void reset() override {}
};

void definitions()
{
    PlayerCharacter player({100, 200});
    Enemy enemy({300, 400});
    check(player.team() == elysia::gameplay::collision::teams::Player, "player team");
    check(enemy.team() == elysia::gameplay::collision::teams::Enemy, "enemy team");
    check(near(player.move_speed(), 200) && near(enemy.move_speed(), 200), "move speeds");
    check(player.size() == elysia::core::Vector2{32, 32}, "player size retained");
    check(enemy.size() == elysia::core::Vector2{30, 30}, "enemy size retained");
    const auto p = std::get<elysia::physics::AabbShape>(player.collider_definitions()[0].shape).local_rect;
    check(near(p.left(), 5.6f) && near(p.top(), 19.84f)
        && near(p.width(), 20.8f) && near(p.height(), 12.16f), "player foot collider retained");
    const auto e = std::get<elysia::physics::AabbShape>(enemy.collider_definitions()[0].shape).local_rect;
    check(e == elysia::core::Rect{0, 0, 30, 30}, "enemy collider aligned with visual");
    for (Character* character : {static_cast<Character*>(&player), static_cast<Character*>(&enemy)})
    {
        check(character->collider_definitions().size() == 1, "single collider definition");
        const auto& collider = character->collider_definitions()[0];
        check(collider.response == elysia::physics::CollisionResponse::Block
            && collider.material.friction == 0 && collider.material.restitution == 0, "common material");
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
    check(commands.size() == 1 && commands[0].command_rect == enemy.render_rect(), "enemy render bounds");
}

void movement()
{
    TestScene scene;
    auto* player = scene.create_and_add_object<PlayerCharacter>(elysia::core::Vector2{100, 100});
    auto* enemy = scene.create_and_add_object<Enemy>(elysia::core::Vector2{1000, 1000});
    check(scene.physics_world().registered_object_count() == 2
        && scene.physics_world().registered_collider_count() == 2, "one registration per character");
    auto input = [&](std::initializer_list<elysia::input::RawInputControl> keys) {
        elysia::input::RawInputFrame frame;
        for (auto key : keys) frame.state.set_pressed(key, true);
        scene.on_input(frame, {});
        scene.on_update(1.0 / 60.0);
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
    T scene;
    for (int i = 0; i < 3; ++i)
    {
        std::cerr << "lifecycle " << i << " enter\n";
        scene.on_enter({});
        std::cerr << "entered\n";
        scene.on_enter({});
        check(count<Enemy>(scene) == 1 && count<PlayerCharacter>(scene) == 1
            && count<Character>(scene) == 2, "one enemy and player per scene");
        if (i == 1) scene.reset();
        else scene.on_exit();
        std::cerr << "exited\n";
        check(count<Character>(scene) == 0, "exit/reset clears characters");
        scene.on_update(0); // Release destroyed objects; repeated cleanup must remain safe.
        std::cerr << "updated\n";
        scene.reset();
    }
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
        definitions();
        movement();
        scene_lifecycle<GameScene>();
        scene_lifecycle<RoomScene>();
        std::cout << "Character tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
