#pragma once
#include "../../../thirdparty/nlohmann/json.hpp"
#include "strict_json.h"

#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace elysia::io
{
using json = nlohmann::json;

// 统一返回 JSON 读取结果，并在失败时附带可直接上抛/记录的错误信息
struct JsonReadResult
{
    bool success = false;
    std::string error;

    explicit operator bool() const { return success; }
};

// 负责打开单个 JSON 文件、缓存根节点，并提供常见的按 key 读取接口
// 这个类本身不关心业务含义，只处理“文件是否能打开/解析”和“值是否能按预期类型读出”
class JsonLoader
{
public:
    // 读取并解析文件；成功后可通过 root()/get*() 访问内容
    [[nodiscard]] std::expected<void,JsonFileFailure> open_file(
        const std::filesystem::path& path,
        std::source_location origin = std::source_location::current());

    // 清空当前已加载内容，回到未加载状态
    void reset();

    // 当前是否已经成功加载过一个 JSON 文件
    bool is_loaded() const;

    // 返回当前缓存的根节点；调用方通常应先确认 is_loaded()
    const json& root() const;

    // 从根节点读取一个基础值或可直接反序列化的值
    template<typename T>
    JsonReadResult get(std::string_view key, T& out) const;

    // 从指定对象节点读取一个值，适合分层解析时继续向下取字段
    template<typename T>
    JsonReadResult get(
        const json& node,
        std::string_view key,
        T& out
    ) const;

    // 从根节点读取数组，并逐项转换为目标类型
    template<typename T>
    JsonReadResult get_array(std::string_view key, std::vector<T>& out) const;

    // 从指定对象节点读取数组
    template<typename T>
    JsonReadResult get_array(
        const json& node,
        std::string_view key,
        std::vector<T>& out
    ) const;

    // 读取失败时返回默认值，适合可选字段
    template<typename T>
    T get_or(std::string_view key, T default_value) const;

    // 从指定对象节点读取可选字段；失败时返回默认值
    template<typename T>
    T get_or(
        const json& node,
        std::string_view key,
        T default_value
    ) const;

    // 读取一个子对象，并返回其节点指针供上层继续解析
    JsonReadResult get_object(
        std::string_view key,
        const json*& out
    ) const;

    JsonReadResult get_object(
        const json& node,
        std::string_view key,
        const json*& out
    ) const;

private:
    // 将错误追加到结果中，允许同一次失败携带多条上下文
    static void add_error_message(JsonReadResult& result,
        const std::string& error);

    // 负责把单个 json 值转换为目标类型，屏蔽异常细节
    template<typename T>
    bool read_value(const json& value, T& out) const;

private:
    json _root;
    std::filesystem::path _path;
    bool _loaded = false;
};

template<typename T>
JsonReadResult JsonLoader::get(std::string_view key, T& out) const
{
    JsonReadResult result;

    if (!_loaded)
    {
        add_error_message(result, "JSON is not loaded.");
        return result;
    }

    return get(_root, key, out);
}

template<typename T>
JsonReadResult JsonLoader::get(
    const json& node,
    std::string_view key,
    T& out
) const
{
    JsonReadResult result;

    if (!node.is_object())
    {
        add_error_message(result, "JSON node is not an object.");
        return result;
    }

    const std::string key_string(key);

    if (!node.contains(key_string))
    {
        add_error_message(result, "JSON key missing: " + key_string);
        return result;
    }

    if (!read_value(node.at(key_string), out))
    {
        add_error_message(result, "JSON value type mismatch: " + key_string);
        return result;
    }

    result.success = true;
    return result;
}

template<typename T>
JsonReadResult JsonLoader::get_array(std::string_view key, std::vector<T>& out) const
{
    JsonReadResult result;

    if (!_loaded)
    {
        add_error_message(result, "JSON is not loaded.");
        return result;
    }

    return get_array(_root, key, out);
}

template<typename T>
JsonReadResult JsonLoader::get_array(
    const json& node,
    std::string_view key,
    std::vector<T>& out
) const
{
    JsonReadResult result;

    if (!node.is_object())
    {
        add_error_message(result, "JSON node is not an object.");
        return result;
    }

    const std::string key_string(key);

    if (!node.contains(key_string))
    {
        add_error_message(result, "JSON key missing: " + key_string);
        return result;
    }

    const json& value = node.at(key_string);

    if (!value.is_array())
    {
        add_error_message(result, "JSON value is not an array: " + key_string);
        return result;
    }

    std::vector<T> values;
    values.reserve(value.size());

    for (const json& item : value)
    {
        T item_value{};
        if (!read_value(item, item_value))
        {
            add_error_message(result, "JSON array item type mismatch: " + key_string);
            return result;
        }
        values.push_back(std::move(item_value));
    }

    out = std::move(values);
    result.success = true;
    return result;
}

template<typename T>
T JsonLoader::get_or(std::string_view key, T default_value) const
{
    T value = default_value;
    return get(key, value) ? value : default_value;
}

template<typename T>
T JsonLoader::get_or(
    const json& node,
    std::string_view key,
    T default_value
) const
{
    T value = default_value;
    return get(node, key, value) ? value : default_value;
}

template<typename T>
bool JsonLoader::read_value(const json& value, T& out) const
{
    try
    {
        if constexpr (std::is_same_v<T, std::filesystem::path>)
        {
            if (!value.is_string())
                return false;

            out = value.get<std::string>();
        }
        else
        {
            out = value.get<T>();
        }
    }
    catch (const std::exception&)
    {
        return false;
    }

    return true;
}

}
