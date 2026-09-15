#pragma once

#include "../user_config_types.h"

#include <expected>
#include <filesystem>
#include <string>

namespace elysia::config
{
class UserConfigStore
{
public:
    [[nodiscard]] std::expected<UserConfigLoadResult,UserConfigFailure> load(
        const std::filesystem::path& path,
        const UserConfigData& defaults) const;
    [[nodiscard]] std::expected<void,UserConfigFailure> save(
        const std::filesystem::path& path,
        const UserConfigData& settings) const;
};
}
