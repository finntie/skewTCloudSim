#pragma once

#include <fstream>
#include <glm/glm.hpp>
#include <magic_enum/magic_enum.hpp>
#include <tinygltf/json.hpp>
#include <visit_struct/visit_struct.hpp>

#include "core/engine.hpp"
#include "core/fileio.hpp"
#include "tools/log.hpp"

namespace bee
{

/// <summary>
/// Serializers a class instance into a JSON object, provided the class has a VISITABLE_STRUCT macro defined for its members.
/// </summary>
class JsonSerializer
{
public:
    template <typename T, std::enable_if_t<visit_struct::traits::is_visitable<T>::value, bool> = true>
    inline static bool Serialize(const T& value, bee::FileIO::Directory directory, const std::string& name)
    {
        JsonSerializer js;
        visit_struct::for_each(value, js);

        auto filename = Engine.FileIO().GetPath(directory, name);
        std::ofstream ofs(filename);
        if (ofs.is_open())
        {
            ofs << js.m_json.dump(4);
            ofs.close();
            return true;
        }
        return false;
    }

    template <typename T>
    inline void operator()(const char* name, T& v)
    {
        m_json[name] = serialize(v);
    }

private:
    nlohmann::json m_json;
    JsonSerializer() : m_json(nlohmann::json()) {}

    inline static nlohmann::json serialize(bool b) { return b; }
    inline static nlohmann::json serialize(const glm::vec2& v) { return nlohmann::json::array({v.x, v.y}); }
    inline static nlohmann::json serialize(const glm::vec3& v) { return nlohmann::json::array({v.x, v.y, v.z}); }
    inline static nlohmann::json serialize(const glm::vec4& v) { return nlohmann::json::array({v.x, v.y, v.z, v.w}); }
    inline static nlohmann::json serialize(const std::string& value) { return value; }

    // Numbers
    template <typename T, std::enable_if_t<std::is_arithmetic<T>::value, bool> = true>
    inline static nlohmann::json serialize(const T& value)
    {
        return value;
    }

    // Enums
    template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
    inline static nlohmann::json serialize(const T& value)
    {
        return magic_enum::enum_name(value);
    }

    // Vectors
    template <typename T>
    inline static nlohmann::json serialize(const std::vector<T>& value)
    {
        auto result = nlohmann::json::array();
        for (const T& element : value) result.push_back(serialize(element));
        return result;
    }

    // Structs
    template <typename T, std::enable_if_t<visit_struct::traits::is_visitable<T>::value, bool> = true>
    inline static nlohmann::json serialize(const T& value)
    {
        JsonSerializer js;
        visit_struct::for_each(value, js);
        return js.m_json;
    }
};

/// <summary>
/// Deserializers a JSON object into a class instance, provided the class has a VISITABLE_STRUCT macro defined for its members.
/// </summary>
class JsonDeserializer
{
public:
    template <typename T, std::enable_if_t<visit_struct::traits::is_visitable<T>::value, bool> = true>
    inline static bool Deserialize(T& result, bee::FileIO::Directory directory, const std::string& name)
    {
        const std::string& contents = Engine.FileIO().ReadTextFile(directory, name);
        if (!contents.empty())
        {
            const auto& json = nlohmann::json::parse(contents);
            JsonDeserializer jd(json);
            visit_struct::for_each(result, jd);
            return true;
        }
        return false;
    }

    template <typename T>
    inline void operator()(const char* name, T& v) const
    {
        if (m_json.contains(name))
        {
            deserialize(m_json.at(name), v);
        }
        else
        {
            Log::Warn("JSON object {0} does not have an element named {1}", m_json.dump(), name);
        }
    }

private:
    const nlohmann::json& m_json;
    JsonDeserializer(const nlohmann::json& j) : m_json(j) {}

    static void deserialize(const nlohmann::json& j, bool& v) { v = j; }

    inline static void deserialize(const nlohmann::json& j, glm::vec2& v)
    {
        const auto& arr = static_cast<const nlohmann::json::array_t&>(j);
        v.x = arr[0];
        v.y = arr[1];
    }

    inline static void deserialize(const nlohmann::json& j, glm::vec3& v)
    {
        const auto& arr = static_cast<const nlohmann::json::array_t&>(j);
        v.x = arr[0];
        v.y = arr[1];
        v.z = arr[2];
    }

    inline static void deserialize(const nlohmann::json& j, glm::vec4& v)
    {
        const auto& arr = static_cast<const nlohmann::json::array_t&>(j);
        v.x = arr[0];
        v.y = arr[1];
        v.z = arr[2];
        v.w = arr[3];
    }

    inline static void deserialize(const nlohmann::json& j, std::string& v) { v = j; }

    // Numbers
    template <typename T, std::enable_if_t<std::is_arithmetic<T>::value, bool> = true>
    inline static void deserialize(const nlohmann::json& j, T& v)
    {
        v = j;
    }

    // Enums
    template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
    inline static void deserialize(const nlohmann::json& j, T& v)
    {
        std::optional<T> t = magic_enum::enum_cast<T>(std::string(j), magic_enum::case_insensitive);
        if (t.has_value())
        {
            v = t.value();
        }
        else
        {
            Log::Warn("Could not deserialize JSON value {0} into an enum of type {1}",
                      j.dump(),
                      magic_enum::enum_type_name<T>());
        }
    }

    // Vectors
    template <typename T>
    inline static void deserialize(const nlohmann::json& j, std::vector<T>& v)
    {
        v.clear();
        for (auto& element : j)
        {
            v.push_back(T());
            deserialize(element, v.back());
        }
    }

    // Structs
    template <typename T, std::enable_if_t<visit_struct::traits::is_visitable<T>::value, bool> = true>
    inline static void deserialize(const nlohmann::json& j, T& value)
    {
        JsonDeserializer sub(j);
        visit_struct::for_each(value, sub);
    }
};

}  // namespace bee
