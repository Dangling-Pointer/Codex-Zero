#pragma once

#include <cstdint>
#include <string>

namespace elysia::save
{
enum class SaveError
{
    NotInitialized,
    AlreadyInitialized,
    InvalidSaveName,
    InvalidKey,
    InvalidValue,
    NotOpen,
    AlreadyOpen,
    NotFound,
    AlreadyExists,
    KeyNotFound,
    TypeMismatch,
    InvalidDocument,
    UnsupportedFormatVersion,
    IoFailure,
    DirtySave
};

struct SaveFailure
{
    SaveError error = SaveError::InvalidDocument;
    std::string save_name;
    std::string key;
    std::string message;
};

struct SaveOpenResult
{
    bool recovered = false;
    std::string warning;
};

enum class SaveCreateMode
{
    FailIfExists,
    OverwriteExisting
};

enum class SaveClosePolicy
{
    RejectIfDirty,
    DiscardChanges
};
}
