#include "room_scene.h"

#include "../characters/player_character.h"
#include "../characters/character.h"

#include "scene_keys.h"

#include "../../engine/core/render/colors.h"
#include "../../engine/core/render/render_command.h"
#include "../../engine/physics/tile/tile_collision_world.h"

#include "../../engine/tools/debug_draw.h"

namespace
{
    constexpr int kRoomColumns = 20;
    constexpr int kRoomRows = 12;
    constexpr float kTileSize = 64.0f;

    class RoomBoundaryWorld final : public elysia::physics::ITileCollisionWorld
    {
    public:
        [[nodiscard]] elysia::core::Vector2 world_origin() const noexcept override { return {}; }
        [[nodiscard]] elysia::core::Vector2 tile_size() const noexcept override { return {kTileSize, kTileSize}; }
        [[nodiscard]] int columns() const noexcept override { return kRoomColumns; }
        [[nodiscard]] int rows() const noexcept override { return kRoomRows; }
        [[nodiscard]] elysia::physics::TileOutOfBoundsPolicy out_of_bounds_policy() const noexcept override
        {
            return elysia::physics::TileOutOfBoundsPolicy::Empty;
        }
        [[nodiscard]] elysia::physics::TileCollisionCell cell_at(
            elysia::physics::TileCoordinate coordinate) const noexcept override
        {
            const bool is_boundary = coordinate.x == 0 || coordinate.x == kRoomColumns - 1 || coordinate.y == 0 || coordinate.y == kRoomRows - 1;
            return {.type = is_boundary ? elysia::physics::TileCollisionType::Block
                                        : elysia::physics::TileCollisionType::Empty};
        }
    };

    class RoomBoundaryVisual final : public elysia::core::GameObject
    {
    public:
        RoomBoundaryVisual() : GameObject(elysia::core::DepthLayer::Background) {}

        void submit_render_commands(std::vector<elysia::core::RenderCommand> &out_commands) const override
        {
            constexpr float width = kRoomColumns * kTileSize;
            constexpr float height = kRoomRows * kTileSize;
            const auto color = elysia::core::colors::gray_700;
            out_commands.push_back(elysia::core::make_world_fill_rect_command({0, 0, width, kTileSize}, color));
            out_commands.push_back(elysia::core::make_world_fill_rect_command({0, height - kTileSize, width, kTileSize}, color));
            out_commands.push_back(elysia::core::make_world_fill_rect_command({0, kTileSize, kTileSize, height - 2 * kTileSize}, color));
            out_commands.push_back(elysia::core::make_world_fill_rect_command({width - kTileSize, kTileSize, kTileSize, height - 2 * kTileSize}, color));
        }
    };

    RoomBoundaryWorld kRoomBoundaryWorld;
}

void RoomScene::on_enter(const elysia::scene::ScenePayload &payload)
{
    (void)payload;
    if (_player)
        return;

    (void)physics_world().set_tile_world(kRoomBoundaryWorld);
    _room_boundary = create_and_add_object<RoomBoundaryVisual>();
    _player = create_and_add_object<PlayerCharacter>(
        elysia::core::Vector2{(kRoomColumns * kTileSize - 64.0f) * 0.5f,
                              (kRoomRows * kTileSize - 64.0f) * 0.5f});

    (void)create_and_add_object<Character>(elysia::core::Vector2{100, 100});

    ELYSIA_DEBUG_DRAW->set_enabled(true);
    ELYSIA_DEBUG_DRAW->set_enabled_categories(elysia::tools::DebugDrawCategory::All);
}

void RoomScene::on_exit()
{
    ELYSIA_DEBUG_DRAW->set_enabled(false);
    clear_room();
}
void RoomScene::reset() { clear_room(); }

void RoomScene::on_input(const elysia::input::RawInputFrame &input,
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
    }
}

std::optional<elysia::core::Rect> RoomScene::resolve_camera_focus_rect() const
{
    return _player && !_player->is_destroyed()
               ? std::optional<elysia::core::Rect>{_player->world_rect()}
               : std::nullopt;
}

void RoomScene::clear_room() noexcept
{
    if (_player)
        _player->destroy();
    if (_room_boundary)
        _room_boundary->destroy();
    _player = nullptr;
    _room_boundary = nullptr;
    (void)physics_world().clear_tile_world(kRoomBoundaryWorld);
}
