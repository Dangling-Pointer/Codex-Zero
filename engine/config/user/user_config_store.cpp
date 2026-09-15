#include "user_config_store.h"

#include "user_config_json_fields.h"
#include "../../io/json/strict_json.h"

#include <chrono>
#include <fstream>
#include <set>

namespace elysia::config
{
namespace
{
using Data = UserConfigData;
using Json = elysia::io::json;
constexpr int k_schema_version = 2;

enum class ParseKind { Valid, Invalid, Future };
struct ParseResult
{
    ParseKind kind = ParseKind::Invalid;
    Data data;
    std::string error;
    std::string pointer;
};

bool allowed_fields(const Json& object,std::initializer_list<std::string_view> fields,
    bool require_all,std::string_view path,std::string& error)
{
    if (!object.is_object()) { error = std::string(path) + " must be an object."; return false; }
    std::set<std::string> allowed;
    for (auto field : fields) allowed.emplace(field);
    for (const auto& [key,value] : object.items())
        if (!allowed.contains(key)) { error = "Unknown UserConfig field: " + std::string(path) + "." + key; return false; }
    if (require_all) for (const auto& key : allowed)
        if (!object.contains(key)) { error = "Missing UserConfig field: " + std::string(path) + "." + key; return false; }
    return true;
}

ParseResult parse(const std::filesystem::path& path,const Data& defaults)
{
    const auto loaded = elysia::io::load_strict_json(path);
    if (!loaded) return {ParseKind::Invalid,{},loaded.error().message,
        loaded.error().json_pointer};
    const Json& root = *loaded;
    if (!root.is_object()) return {ParseKind::Invalid,{},"UserConfig root must be an object.","/"};
    if (!root.contains("schema_version")
        || !root.at("schema_version").is_number_integer())
        return {ParseKind::Invalid,{},"UserConfig schema_version must be an integer.","/schema_version"};
    const int version = root.at("schema_version").get<int>();
    if (version > k_schema_version)
        return {ParseKind::Future,{},"UserConfig schema_version is newer than this application.","/schema_version"};
    if (version != k_schema_version)
        return {ParseKind::Invalid,{},"Unsupported UserConfig schema_version.","/schema_version"};
    Data data = defaults;
    std::string error;
    if (!allowed_fields(
            root,
            {"schema_version","window","render","audio","localization"},
            true,
            "root",
            error))
        return {ParseKind::Invalid,{},error,"/"};
    auto section = [&](const char* name,const Json*& out)
    {
        if (!root.at(name).is_object()) { error = std::string(name) + " must be an object."; return false; }
        out = &root.at(name); return true;
    };
    const Json* node = nullptr;
    if (!section("window",node)) return {ParseKind::Invalid,{},error,"/window"};
    if (!allowed_fields(*node,{"mode","windowed_size"},true,"window",error)
        || !detail::parse_window_mode(
            node->at("mode"),
            data.window.mode,
            error)
        || !allowed_fields(
            node->at("windowed_size"),
            {"width","height"},
            true,
            "window.windowed_size",
            error)
        || !detail::parse_positive_int(
            node->at("windowed_size"),
            "width",
            data.window.windowed_size.width,
            error)
        || !detail::parse_positive_int(
            node->at("windowed_size"),
            "height",
            data.window.windowed_size.height,
            error))
        return {ParseKind::Invalid,{},error,"/window"};
    node = nullptr;
    if (!section("render",node)) return {ParseKind::Invalid,{},error,"/render"};
    if (!allowed_fields(*node,{"fps","vsync"},true,"render",error))
        return {ParseKind::Invalid,{},error,"/render"};
    if (!detail::parse_positive_number(
            *node,
            "fps",
            data.target_fps,
            error)
        || !detail::parse_boolean(*node,"vsync",data.vsync,error))
        return {ParseKind::Invalid,{},error,"/render"};
    node = nullptr;
    if (!section("audio",node)) return {ParseKind::Invalid,{},error,"/audio"};
    if (!allowed_fields(*node,{"master_volume","music_volume","sound_volume"},true,"audio",error)
        || !detail::parse_volume(*node,"master_volume",data.audio.master_volume,error)
        || !detail::parse_volume(*node,"music_volume",data.audio.music_volume,error)
        || !detail::parse_volume(*node,"sound_volume",data.audio.sound_volume,error))
        return {ParseKind::Invalid,{},error,"/audio"};
    node = nullptr;
    if (!section("localization",node)) return {ParseKind::Invalid,{},error,"/localization"};
    if (!allowed_fields(*node,{"language"},true,"localization",error))
        return {ParseKind::Invalid,{},error,"/localization"};
    if (!detail::parse_non_empty_string(
            *node,
            "language",
            data.language,
            error))
        return {ParseKind::Invalid,{},error,"/localization/language"};
    return {ParseKind::Valid,std::move(data),{}, {}};
}

Json serialize(const Data& data)
{
    const char* mode = "invalid";
    switch (data.window.mode)
    {
    case WindowMode::Windowed:
        mode = "windowed";
        break;
    case WindowMode::BorderlessFullscreen:
        mode = "borderless_fullscreen";
        break;
    }
    return {{"schema_version",k_schema_version},
        {"window",{
            {"mode",mode},
            {"windowed_size",{
                {"width",data.window.windowed_size.width},
                {"height",data.window.windowed_size.height}
            }}
        }},
        {"render",{{"fps",data.target_fps},{"vsync",data.vsync}}},
        {"audio",{{"master_volume",data.audio.master_volume},{"music_volume",data.audio.music_volume},{"sound_volume",data.audio.sound_volume}}},
        {"localization",{{"language",data.language}}}};
}

UserConfigFailure failure(
    UserConfigError kind,std::string message,
    std::filesystem::path path = {},std::string pointer = {},
    std::source_location origin = std::source_location::current())
{
    const std::string reason = message;
    auto diagnostic = elysia::core::make_failure_diagnostic(
        message,{elysia::core::make_failure_diagnostic_entry(
            "user-config",{},path,path,std::move(pointer),reason,origin)},origin);
    return {kind,{},std::move(message),std::move(diagnostic)};
}
}

std::expected<void,UserConfigFailure> UserConfigStore::save(const std::filesystem::path& path,const Data& data) const
{
    const std::filesystem::path tmp = path.string() + ".tmp";
    const std::filesystem::path bak = path.string() + ".bak";
    std::error_code error;
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path(),error);
        if (error) return std::unexpected(failure(UserConfigError::SaveFailed,
            "UserConfig directory creation failed: " + error.message(),path));
    }
    {
        std::ofstream output(tmp,std::ios::trunc);
        if (!output) return std::unexpected(failure(UserConfigError::SaveFailed,
            "Temporary UserConfig could not be opened.",tmp));
        output << serialize(data).dump(2);
        if (!output.good()) return std::unexpected(failure(UserConfigError::SaveFailed,
            "Temporary UserConfig could not be written.",tmp));
    }
    const auto verified = parse(tmp,data);
    if (verified.kind != ParseKind::Valid)
        return std::unexpected(failure(UserConfigError::SaveFailed,
            "Temporary UserConfig verification failed: " + verified.error,tmp));

    const bool had_backup = std::filesystem::exists(bak,error);
    if (error) return std::unexpected(failure(UserConfigError::SaveFailed,
        "UserConfig backup status failed: " + error.message(),bak));
    if (had_backup)
    {
        std::filesystem::remove(bak,error);
        if (error) return std::unexpected(failure(UserConfigError::SaveFailed,
            "UserConfig backup removal failed: " + error.message(),bak));
    }
    const bool had_primary = std::filesystem::exists(path,error);
    if (error) return std::unexpected(failure(UserConfigError::SaveFailed,
        "UserConfig status failed: " + error.message(),path));
    if (had_primary)
    {
        std::filesystem::rename(path,bak,error);
        if (error) return std::unexpected(failure(UserConfigError::SaveFailed,
            "UserConfig backup rename failed: " + error.message(),path));
    }
    std::filesystem::rename(tmp,path,error);
    if (!error) return {};

    const std::string rename_reason = error.message();
    if (had_primary)
    {
        std::error_code rollback_error;
        const bool backup_exists = std::filesystem::exists(bak,rollback_error);
        const bool primary_exists = std::filesystem::exists(path,rollback_error);
        if (!rollback_error && backup_exists && !primary_exists)
            std::filesystem::rename(bak,path,rollback_error);
    }
    return std::unexpected(failure(UserConfigError::SaveFailed,
        "Atomic UserConfig replacement failed: " + rename_reason,path));
}

std::expected<UserConfigLoadResult,UserConfigFailure> UserConfigStore::load(
    const std::filesystem::path& path,const Data& defaults) const
{
    UserConfigLoadResult result;
    const std::filesystem::path tmp = path.string() + ".tmp";
    const std::filesystem::path bak = path.string() + ".bak";
    std::error_code status_error;
    const bool primary_exists = std::filesystem::exists(path,status_error);
    if (status_error) return std::unexpected(failure(UserConfigError::LoadFailed,
        "UserConfig status failed: " + status_error.message(),path));
    if (primary_exists)
    {
        const auto primary = parse(path,defaults);
        if (primary.kind == ParseKind::Future) return std::unexpected(failure(
            UserConfigError::LoadFailed,primary.error,path,primary.pointer));
        if (primary.kind == ParseKind::Valid)
        {
            result.settings = primary.data;
            return result;
        }
        result.warning = failure(UserConfigError::LoadFailed,primary.error,path,primary.pointer);
        const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const std::filesystem::path corrupt = path.string() + "." + std::to_string(stamp) + ".corrupt";
        std::error_code archive_error;
        std::filesystem::rename(path,corrupt,archive_error);
        if (archive_error)
        {
            result.warning->message += " Corrupt UserConfig could not be archived: "
                + archive_error.message();
            result.warning->diagnostic.message = result.warning->message;
        }
    }
    for (const auto& candidate : {tmp,bak})
    {
        const bool candidate_exists = std::filesystem::exists(candidate,status_error);
        if (status_error) return std::unexpected(failure(UserConfigError::LoadFailed,
            "UserConfig recovery candidate status failed: " + status_error.message(),candidate));
        if (!candidate_exists) continue;
        const auto recovered = parse(candidate,defaults);
        if (recovered.kind == ParseKind::Future) return std::unexpected(failure(
            UserConfigError::LoadFailed,recovered.error,candidate,recovered.pointer));
        if (recovered.kind == ParseKind::Valid)
        {
            result.settings = recovered.data; result.recovered = true;
            if (auto saved = save(path,result.settings); !saved) return std::unexpected(saved.error());
            return result;
        }
    }
    result.settings = defaults; result.rebuilt = true;
    if (auto saved = save(path,result.settings); !saved) return std::unexpected(saved.error());
    return result;
}
}
