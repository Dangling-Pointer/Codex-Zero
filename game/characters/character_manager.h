#pragma once

#include "character_context.h"
#include "engine/tools/singleton.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>

struct CharacterDefinition
{
    CharacterId id;
    std::string display_name;
    float base_max_health = 0.0f;
    float base_move_speed = 0.0f;
};

// Read-only player definitions
class CharacterManager final : public elysia::tools::Singleton<CharacterManager>
{
    friend class elysia::tools::Singleton<CharacterManager>;

public:
    [[nodiscard]] std::span<const CharacterDefinition> definitions() const noexcept;

    // Returned pointers remain valid for the lifetime of the singleton
    [[nodiscard]] const CharacterDefinition* find_definition(std::string_view id) const noexcept;

    [[nodiscard]] std::optional<CharacterContext> create_context(std::string_view id) const;

private:
    CharacterManager();

    const std::array<CharacterDefinition, 1> _definitions;
};
