#include "i18n_manifest_loader.h"

#include "../json/json_loader.h"
#include <utility>

namespace elysia::io
{
std::expected<I18nManifest,ManifestLoadFailure> I18nManifestLoader::load(
	const std::filesystem::path& manifest_path) const
{
	const auto fail = [&manifest_path](ManifestLoadError code,std::string message,
		std::string pointer = {},std::source_location origin = std::source_location::current())
		-> std::expected<I18nManifest,ManifestLoadFailure>
	{
		return std::unexpected(make_manifest_load_failure(
			code,std::move(message),"i18n-manifest",{},manifest_path,manifest_path,
			std::move(pointer),origin));
	};
	if (auto source = validate_manifest_source(manifest_path,"i18n-manifest");
		!source)
		return std::unexpected(std::move(source.error()));
	JsonLoader loader;
	const auto open_result = loader.open_file(manifest_path);
	if (!open_result)
		return std::unexpected(manifest_failure_from_json(
			open_result.error(),"i18n-manifest","Load i18n manifest failed: "));

	if (!loader.root().is_object())
		return fail(ManifestLoadError::InvalidDocument,
			"Load i18n manifest failed: root is not an object.");

	I18nManifest parsed_manifest;
	if (!loader.get("default_language", parsed_manifest.default_language)
		|| parsed_manifest.default_language.empty())
		return fail(ManifestLoadError::MissingField,
			"Load i18n manifest failed: default_language is missing or invalid.",
			"/default_language");

	if (!loader.get_array("languages", parsed_manifest.languages)
		|| parsed_manifest.languages.empty())
		return fail(ManifestLoadError::MissingField,
			"Load i18n manifest failed: languages is missing or invalid.","/languages");

	if (!loader.root().contains("file") || !loader.root().at("file").is_array())
		return fail(ManifestLoadError::MissingField,
			"Load i18n manifest failed: file is missing or not an array.","/file");

	std::size_t file_index = 0;
	for (const json& file_node : loader.root().at("file"))
	{
		const std::string pointer = "/file/" + std::to_string(file_index++);
		if (!file_node.is_string())
			return fail(ManifestLoadError::InvalidField,
				"Load i18n manifest failed: file entry is not a string.",pointer);

		const std::string file_path = file_node.get<std::string>();
		if (file_path.empty())
			return fail(ManifestLoadError::InvalidValue,
				"Load i18n manifest failed: file entry is empty.",pointer);

		parsed_manifest.files.push_back({
			file_path,elysia::resources::make_resource_origin(
				manifest_path,pointer,{},"i18n",{},file_path)});
	}

	if (parsed_manifest.files.empty())
		return fail(ManifestLoadError::MissingContent,
			"Load i18n manifest failed: file list is empty.","/file");

	return parsed_manifest;
}

}
