#pragma once

#include "user_config_data.h"
#include "../core/diagnostics/failure_diagnostic.h"

#include <optional>
#include <string>

namespace elysia::config
{
enum class UserConfigError
{
    InvalidValue,
    ChangeHandlerUnavailable,
    RuntimeApplyFailed,
    SaveFailed,
    LoadFailed
};

struct UserConfigFailure
{
    UserConfigError error = UserConfigError::InvalidValue;
    std::string setting_name;
    std::string message;
    elysia::core::FailureDiagnostic diagnostic;
};

struct UserConfigCommitFailure
{
    UserConfigFailure cause;
    std::optional<UserConfigFailure> rollback_failure;
};

struct UserConfigRuntimeState
{
    UserConfigData settings;
    bool restart_required = false;
};

enum class UserConfigApplyStatus
{
    Applied,
    PendingRestart
};

struct UserConfigLoadResult
{
    UserConfigData settings;
    std::optional<UserConfigFailure> warning;
    bool recovered = false;
    bool rebuilt = false;
};
}
