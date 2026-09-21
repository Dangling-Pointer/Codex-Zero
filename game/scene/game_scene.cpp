#include "game_scene.h"

#include "../characters/player_character.h"
#include "../characters/character.h"
#include "../map/dungeon_room.h"

#include "scene_keys.h"

#include "../../engine/tools/debug_draw.h"
#include "../../engine/camera/camera_manager.h"

#include "../combat/projectiles/bullet.h"

void GameScene::on_enter(const elysia::scene::ScenePayload &payload)
{
    (void)payload;

    if (_player)
        return;

    _room = create_and_add_object<DungeonRoom>();
    (void)physics_world().set_tile_world(*_room);
    _player = create_and_add_object<PlayerCharacter>(
        _room->center() - elysia::core::Vector2{32.0f, 32.0f});

    // test character
    (void)create_and_add_object<Character>(_room->center());

    constexpr auto slot = elysia::camera::CameraSlot::Main;
    ELYSIA_CAMERA->set_follow_strategy(
        slot, std::make_unique<elysia::camera::HardFollowStrategy>());
    ELYSIA_CAMERA->set_zoom(slot, 2.0f);
    ELYSIA_CAMERA->set_center(slot, _player->center());

    ELYSIA_DEBUG_DRAW->set_enabled(true);
    ELYSIA_DEBUG_DRAW->set_enabled_categories(elysia::tools::DebugDrawCategory::All);
}

void GameScene::on_exit()
{
    ELYSIA_DEBUG_DRAW->set_enabled(false);
    clear_room();
}

void GameScene::reset() { clear_room(); }

void GameScene::on_input(const elysia::input::RawInputFrame &input,
                         const std::vector<elysia::input::RawInputEvent> &events)
{
    elysia::gameplay::GameplayScene::on_input(input, events);
    for (const elysia::input::RawInputEvent &event : events)
    {
        if (event.type == elysia::input::RawInputEventType::ControlPressed && event.control == elysia::input::RawInputControl::KeyEscape)
        {
            request_scene_switch(MainMenu);
            return;
        }
        if (event.control == elysia::input::RawInputControl::KeyF)
        {
            fire_test_wand();
            return;
        }
    }
}

std::optional<elysia::core::Rect> GameScene::resolve_camera_focus_rect() const
{
    return _player && !_player->is_destroyed()
               ? std::optional<elysia::core::Rect>{_player->world_rect()}
               : std::nullopt;
}

void GameScene::clear_room() noexcept
{
    if (_player)
        _player->destroy();
    if (_room)
    {
        (void)physics_world().clear_tile_world(*_room);
        _room->destroy();
    }
    _player = nullptr;
    _room = nullptr;
}

void GameScene::fire_test_wand()
{
    if (!_player || _player->is_destroyed())
        return;

    const elysia::core::Vector2 direction{1.0f, 0.0f};
    const std::vector<ShotDescriptor> shots = _test_wand.attack(direction);

    for (const ShotDescriptor &shot : shots)
    {
        Bullet_Attributes attributes = shot.bullet_attributes;
        attributes.start_position = _player->center() + shot.spawn_offset;
        attributes.starting_velocity =
            shot.shot_direction * attributes.bullet_speed;

        (void)create_and_add_object<Bullet>(attributes);
    }
}
