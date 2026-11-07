#include "character_manager.h"

CharacterManager::CharacterManager()
    : _definitions{{CharacterDefinition{
          .id = "default_player",
          .display_name = "Default Player",
          .base_max_health = 100.0f,
          .base_move_speed = 200.0f}}}
          //tmp for testing
{
}

std::span<const CharacterDefinition> CharacterManager::definitions() const noexcept
{
    return _definitions;
}

const CharacterDefinition* CharacterManager::find_definition(std::string_view id) const noexcept
{
    for (const auto& definition : _definitions)
    {
        if (definition.id == id)
            return &definition;
    }
    return nullptr;
}

std::optional<CharacterContext> CharacterManager::create_context(std::string_view id) const
{
    const auto* definition = find_definition(id);
    if (!definition)
        return std::nullopt;

    return CharacterContext{
        .character_id = definition->id,
        .health = definition->base_max_health,
        .max_health = definition->base_max_health,
        .move_speed = definition->base_move_speed};
}
