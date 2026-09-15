#include "save_store.h"

#include "../../io/json/strict_json.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <utility>

namespace elysia::save::detail
{
namespace
{
using Json = elysia::io::json;
constexpr std::int64_t k_format_version = 1;

enum class ParseKind
{
    Valid,
    Invalid,
    Future
};

struct ParseResult
{
    ParseKind kind = ParseKind::Invalid;
    SaveData data;
    std::string error;
};

SaveFailure failure(
    SaveError error,
    std::string_view save_name,
    std::string message,
    std::string key = {})
{
    return SaveFailure{
        error,
        std::string(save_name),
        std::move(key),
        std::move(message)
    };
}

bool has_exact_fields(
    const Json& object,
    std::initializer_list<std::string_view> expected)
{
    if (!object.is_object() || object.size() != expected.size()) return false;
    return std::ranges::all_of(expected,[&object](std::string_view field)
    {
        return object.contains(field);
    });
}

std::expected<std::int64_t,std::string> read_int64(const Json& value)
{
    if (!value.is_number_integer())
        return std::unexpected("value must be an int64");
    try
    {
        if (value.is_number_unsigned())
        {
            const auto number = value.get<std::uint64_t>();
            if (number > static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()))
                return std::unexpected("integer is outside the int64 range");
            return static_cast<std::int64_t>(number);
        }
        return value.get<std::int64_t>();
    }
    catch (const std::exception& exception)
    {
        return std::unexpected(exception.what());
    }
}

template<typename Value,typename Reader>
std::expected<std::vector<Value>,std::string> read_array(
    const Json& value,
    Reader&& reader)
{
    if (!value.is_array()) return std::unexpected("value must be an array");
    std::vector<Value> result;
    result.reserve(value.size());
    for (const Json& item : value)
    {
        auto parsed = reader(item);
        if (!parsed) return std::unexpected(parsed.error());
        result.push_back(std::move(*parsed));
    }
    return result;
}

std::expected<bool,std::string> read_bool(const Json& value)
{
    if (!value.is_boolean()) return std::unexpected("value must be a bool");
    return value.get<bool>();
}

std::expected<double,std::string> read_double(const Json& value)
{
    if (!value.is_number_float()) return std::unexpected("value must be a double");
    const double number = value.get<double>();
    if (!std::isfinite(number)) return std::unexpected("double must be finite");
    return number;
}

std::expected<std::string,std::string> read_string(const Json& value)
{
    if (!value.is_string()) return std::unexpected("value must be a string");
    return value.get<std::string>();
}

std::expected<bool,std::string> add_typed_value(
    SaveData& data,
    const std::string& key,
    std::string_view type,
    const Json& value)
{
    auto store = [&data,&key](auto parsed) -> std::expected<bool,std::string>
    {
        if (!parsed) return std::unexpected(parsed.error());
        auto stored = data.set(key,std::move(*parsed));
        if (!stored) return std::unexpected(stored.error().message);
        return *stored;
    };

    if (type == "bool") return store(read_bool(value));
    if (type == "int64") return store(read_int64(value));
    if (type == "double") return store(read_double(value));
    if (type == "string") return store(read_string(value));
    if (type == "bool_array")
        return store(read_array<bool>(value,read_bool));
    if (type == "int64_array")
        return store(read_array<std::int64_t>(value,read_int64));
    if (type == "double_array")
        return store(read_array<double>(value,read_double));
    if (type == "string_array")
        return store(read_array<std::string>(value,read_string));
    return std::unexpected("unknown SaveData type tag: " + std::string(type));
}

ParseResult parse_document(const std::filesystem::path& path)
{
    const auto loaded = elysia::io::load_strict_json(path);
    if (!loaded) return {ParseKind::Invalid,{},loaded.error().message};
    const Json& root = *loaded;
    if (!has_exact_fields(root,{"format_version","types","values"}))
        return {ParseKind::Invalid,{},"Save root must contain exactly format_version, types, and values."};

    const auto version = read_int64(root.at("format_version"));
    if (!version)
        return {ParseKind::Invalid,{},"Save format_version " + version.error() + "."};
    if (*version > k_format_version)
        return {ParseKind::Future,{},"Save format_version is newer than this engine."};
    if (*version != k_format_version)
        return {ParseKind::Invalid,{},"Unsupported Save format_version."};

    const Json& types = root.at("types");
    const Json& values = root.at("values");
    if (!types.is_object() || !values.is_object())
        return {ParseKind::Invalid,{},"Save types and values must be objects."};
    if (types.size() != values.size())
        return {ParseKind::Invalid,{},"Save types and values must contain the same keys."};

    SaveData data;
    for (const auto& [key,type_node] : types.items())
    {
        if (key.empty())
            return {ParseKind::Invalid,{},"SaveData keys must not be empty."};
        if (!values.contains(key))
            return {ParseKind::Invalid,{},"Save types and values must contain the same keys."};
        if (!type_node.is_string())
            return {ParseKind::Invalid,{},"Save type tag for '" + key + "' must be a string."};
        const auto added = add_typed_value(
            data,
            key,
            type_node.get<std::string>(),
            values.at(key));
        if (!added)
            return {ParseKind::Invalid,{},"Invalid SaveData value '" + key + "': " + added.error()};
    }

    return {ParseKind::Valid,std::move(data),{}};
}

std::string_view type_tag(const SaveValue& value)
{
    return std::visit([](const auto& stored) -> std::string_view
    {
        using Value = std::remove_cvref_t<decltype(stored)>;
        if constexpr (std::same_as<Value,bool>) return "bool";
        else if constexpr (std::same_as<Value,std::int64_t>) return "int64";
        else if constexpr (std::same_as<Value,double>) return "double";
        else if constexpr (std::same_as<Value,std::string>) return "string";
        else if constexpr (std::same_as<Value,std::vector<bool>>) return "bool_array";
        else if constexpr (std::same_as<Value,std::vector<std::int64_t>>) return "int64_array";
        else if constexpr (std::same_as<Value,std::vector<double>>) return "double_array";
        else return "string_array";
    },value);
}

Json serialize_document(const SaveData& data)
{
    Json types = Json::object();
    Json values = Json::object();
    for (const auto& [key,value] : data.entries())
    {
        types[key] = type_tag(value);
        std::visit([&values,&key](const auto& stored)
        {
            values[key] = stored;
        },value);
    }
    return Json{
        {"format_version",k_format_version},
        {"types",std::move(types)},
        {"values",std::move(values)}
    };
}

std::filesystem::path make_corrupt_path(const std::filesystem::path& primary)
{
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::filesystem::path candidate = primary.string() + "."
        + std::to_string(stamp) + ".corrupt";
    int suffix = 1;
    while (std::filesystem::exists(candidate))
    {
        candidate = primary.string() + "." + std::to_string(stamp)
            + "." + std::to_string(suffix++) + ".corrupt";
    }
    return candidate;
}

bool is_portable_save_character(unsigned char character)
{
    return (character >= 'a' && character <= 'z')
        || (character >= 'A' && character <= 'Z')
        || (character >= '0' && character <= '9')
        || character == '_'
        || character == '-';
}

bool is_windows_reserved_name(std::string_view name)
{
    std::string uppercase(name);
    std::ranges::transform(uppercase,uppercase.begin(),[](unsigned char character)
    {
        return static_cast<char>(std::toupper(character));
    });
    if (uppercase == "CON" || uppercase == "PRN" || uppercase == "AUX"
        || uppercase == "NUL")
        return true;
    if (uppercase.size() == 4
        && (uppercase.starts_with("COM") || uppercase.starts_with("LPT"))
        && uppercase[3] >= '1' && uppercase[3] <= '9')
        return true;
    return false;
}
}

SaveStore::SaveStore(std::filesystem::path save_directory)
    : _save_directory(std::move(save_directory))
{
}

std::expected<void,SaveFailure> SaveStore::initialize() const
{
    try
    {
        if (_save_directory.empty())
            return std::unexpected(failure(SaveError::IoFailure,{},"Save directory must not be empty."));
        std::filesystem::create_directories(_save_directory);
        if (!std::filesystem::is_directory(_save_directory))
            return std::unexpected(failure(SaveError::IoFailure,{},"Save path is not a directory: " + _save_directory.string()));
        return {};
    }
    catch (const std::exception& exception)
    {
        return std::unexpected(failure(SaveError::IoFailure,{},"Save directory initialization failed: " + std::string(exception.what())));
    }
}

std::expected<void,SaveFailure> SaveStore::validate_save_name(
    std::string_view save_name)
{
    if (save_name.empty() || save_name.size() > 64
        || !std::ranges::all_of(save_name,is_portable_save_character)
        || is_windows_reserved_name(save_name))
    {
        return std::unexpected(failure(
            SaveError::InvalidSaveName,
            save_name,
            "Save name must contain 1-64 ASCII letters, digits, '_' or '-', and must not be a reserved file name."));
    }
    return {};
}

std::filesystem::path SaveStore::primary_path(std::string_view save_name) const
{
    return _save_directory / (std::string(save_name) + ".json");
}

std::filesystem::path SaveStore::temporary_path(std::string_view save_name) const
{
    return primary_path(save_name).string() + ".tmp";
}

std::filesystem::path SaveStore::backup_path(std::string_view save_name) const
{
    return primary_path(save_name).string() + ".bak";
}

std::expected<SaveStoreLoadResult,SaveFailure> SaveStore::load(
    std::string_view save_name) const
{
    if (auto valid = validate_save_name(save_name); !valid)
        return std::unexpected(valid.error());

    try
    {
        const auto primary = primary_path(save_name);
        const auto temporary = temporary_path(save_name);
        const auto backup = backup_path(save_name);
        std::string warning;

        if (std::filesystem::exists(primary))
        {
            auto parsed = parse_document(primary);
            if (parsed.kind == ParseKind::Future)
                return std::unexpected(failure(SaveError::UnsupportedFormatVersion,save_name,parsed.error));
            if (parsed.kind == ParseKind::Valid)
                return SaveStoreLoadResult{std::move(parsed.data),false,{}};

            warning = parsed.error;
            const auto corrupt = make_corrupt_path(primary);
            std::filesystem::rename(primary,corrupt);
            warning += " Corrupt primary was archived as " + corrupt.filename().string() + ".";
        }

        bool found_candidate = false;
        for (const auto& candidate : {temporary,backup})
        {
            if (!std::filesystem::exists(candidate)) continue;
            found_candidate = true;
            auto parsed = parse_document(candidate);
            if (parsed.kind == ParseKind::Future)
                return std::unexpected(failure(SaveError::UnsupportedFormatVersion,save_name,parsed.error));
            if (parsed.kind != ParseKind::Valid)
            {
                if (!warning.empty()) warning += ' ';
                warning += candidate.filename().string() + " is invalid: " + parsed.error;
                continue;
            }

            auto promoted = save(save_name,parsed.data);
            if (!promoted) return std::unexpected(promoted.error());
            return SaveStoreLoadResult{std::move(parsed.data),true,std::move(warning)};
        }

        const std::string message = warning.empty()
            ? (found_candidate ? "No valid recovery save exists." : "Save file was not found.")
            : "Save is corrupt and no valid recovery copy exists. " + warning;
        return std::unexpected(failure(
            warning.empty() && !found_candidate ? SaveError::NotFound : SaveError::InvalidDocument,
            save_name,
            message));
    }
    catch (const std::exception& exception)
    {
        return std::unexpected(failure(SaveError::IoFailure,save_name,"Save load failed: " + std::string(exception.what())));
    }
}

std::expected<void,SaveFailure> SaveStore::save(
    std::string_view save_name,
    const SaveData& data) const
{
    if (auto valid = validate_save_name(save_name); !valid)
        return std::unexpected(valid.error());

    const auto primary = primary_path(save_name);
    const auto temporary = temporary_path(save_name);
    const auto backup = backup_path(save_name);
    try
    {
        std::filesystem::create_directories(_save_directory);
        {
            std::ofstream output(temporary,std::ios::binary | std::ios::trunc);
            if (!output)
                return std::unexpected(failure(SaveError::IoFailure,save_name,"Open temporary save failed: " + temporary.string()));
            output << serialize_document(data).dump(2) << '\n';
            output.flush();
            if (!output.good())
                return std::unexpected(failure(SaveError::IoFailure,save_name,"Write temporary save failed: " + temporary.string()));
        }

        const auto verified = parse_document(temporary);
        if (verified.kind != ParseKind::Valid)
            return std::unexpected(failure(SaveError::InvalidDocument,save_name,"Temporary save verification failed: " + verified.error));

        const bool had_primary = std::filesystem::exists(primary);
        if (had_primary)
        {
            if (std::filesystem::exists(backup)) std::filesystem::remove(backup);
            std::filesystem::rename(primary,backup);
        }

        try
        {
            std::filesystem::rename(temporary,primary);
        }
        catch (...)
        {
            if (had_primary && std::filesystem::exists(backup)
                && !std::filesystem::exists(primary))
                std::filesystem::rename(backup,primary);
            throw;
        }
        return {};
    }
    catch (const std::exception& exception)
    {
        return std::unexpected(failure(SaveError::IoFailure,save_name,"Recoverable save write failed: " + std::string(exception.what())));
    }
}

std::expected<bool,SaveFailure> SaveStore::exists(
    std::string_view save_name) const
{
    if (auto valid = validate_save_name(save_name); !valid)
        return std::unexpected(valid.error());
    try
    {
        return std::filesystem::exists(primary_path(save_name))
            || std::filesystem::exists(temporary_path(save_name))
            || std::filesystem::exists(backup_path(save_name));
    }
    catch (const std::exception& exception)
    {
        return std::unexpected(failure(SaveError::IoFailure,save_name,"Save existence check failed: " + std::string(exception.what())));
    }
}

std::expected<std::vector<std::string>,SaveFailure>
SaveStore::list_save_names() const
{
    try
    {
        std::set<std::string> names;
        for (const auto& entry : std::filesystem::directory_iterator(_save_directory))
        {
            if (!entry.is_regular_file()) continue;
            std::string filename = entry.path().filename().string();
            std::string name;
            if (filename.ends_with(".json.tmp"))
                name = filename.substr(0,filename.size() - 9);
            else if (filename.ends_with(".json.bak"))
                name = filename.substr(0,filename.size() - 9);
            else if (filename.ends_with(".json"))
                name = filename.substr(0,filename.size() - 5);
            else
                continue;

            if (validate_save_name(name)) names.insert(std::move(name));
        }
        return std::vector<std::string>(names.begin(),names.end());
    }
    catch (const std::exception& exception)
    {
        return std::unexpected(failure(SaveError::IoFailure,{},"Save listing failed: " + std::string(exception.what())));
    }
}

std::expected<void,SaveFailure> SaveStore::remove(
    std::string_view save_name) const
{
    if (auto valid = validate_save_name(save_name); !valid)
        return std::unexpected(valid.error());
    try
    {
        bool removed = false;
        for (const auto& path : {
            primary_path(save_name),
            temporary_path(save_name),
            backup_path(save_name)})
        {
            if (std::filesystem::exists(path))
            {
                std::filesystem::remove(path);
                removed = true;
            }
        }
        if (!removed)
            return std::unexpected(failure(SaveError::NotFound,save_name,"Save file was not found."));
        return {};
    }
    catch (const std::exception& exception)
    {
        return std::unexpected(failure(SaveError::IoFailure,save_name,"Save removal failed: " + std::string(exception.what())));
    }
}
}
