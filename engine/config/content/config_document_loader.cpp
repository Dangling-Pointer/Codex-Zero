#include "config_document_loader.h"

#include "config_load_utils.h"
#include "../../io/json/strict_json.h"

namespace elysia::config
{
std::expected<ConfigDocument,ConfigLoadFailure> ConfigDocumentLoader::load(
    const ConfigManifestEntry& entry) const
{
    const auto parsed = elysia::io::load_strict_json(entry.document_path);
    const std::string source = config_project_relative(entry.document_path);
    if (!parsed)
    {
        const auto& json_failure = parsed.error();
        if (json_failure.code == elysia::io::JsonFileError::DuplicateProperty)
        {
            ConfigOrigin origin{source,json_failure.json_pointer,entry.key_namespace,
                entry.key_namespace+"."+json_failure.duplicate_property};
            return std::unexpected(make_config_load_failure(ConfigLoadError::DuplicateKey,
                json_failure.message,origin,origin,json_failure.origin));
        }
        const ConfigLoadError code = json_failure.code == elysia::io::JsonFileError::FileMissing
            ? ConfigLoadError::FileMissing
            : json_failure.code == elysia::io::JsonFileError::FilesystemAccess
                ? ConfigLoadError::FilesystemAccess : ConfigLoadError::OpenFailed;
        return std::unexpected(make_config_load_failure(code,json_failure.message,
            entry.origin,{},json_failure.origin));
    }
    return ConfigDocument{entry.key_namespace,entry.document_path,*parsed,
        ConfigOrigin{source,"",entry.key_namespace,entry.key_namespace}};
}
}
