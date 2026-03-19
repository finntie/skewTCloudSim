#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "core/resource.hpp"
#include <cassert>

namespace bee
{

// Forward declaration
class Resource;
class EngineClass;

/// <summary>
/// The Resources class is responsible for loading and unloading resources.
/// </summary>
class Resources
{
public:
    /// <summary>
    /// Load a resource from a file, if the resource is already loaded, it will
    /// return the loaded resource.
    /// </summary>
    template <typename T, typename... Args>
    std::shared_ptr<T> Load(Args&&... args);

    /// <summary>
    /// Create a new resource.
    /// </summary>
    template <typename T, typename... Args>
    std::shared_ptr<T> Create(Args&&... args);

    /// <summary>
    /// Get a loaded resource, if the resource is not loaded, it will return nullptr
    /// </summary>
    template <typename T>
    std::shared_ptr<T> Find(size_t id);

    /// <summary>
    /// Clean up, will unload all the resources that are not being used.
    /// Call this when switching scenes. Otherwise the resources will not be unloaded.
    /// </summary>
    void CleanUp();

private:
    friend class bee::EngineClass;

    /// A map of resources
    std::unordered_map<size_t, std::shared_ptr<Resource>> m_resources;
};

template <typename T, typename... Args>
inline std::shared_ptr<T> Resources::Load(Args&&... args)
{
    const std::string path = T::GetPath(args...);
    const auto id = std::hash<std::string>()(path);

    auto resource = Find<T>(id);
    if (resource) return resource;

    m_resources[id] = std::make_shared<T>(std::forward<Args>(args)...);
    m_resources[id]->m_ID = id;
    m_resources[id]->m_path = path;

    return std::dynamic_pointer_cast<T>(m_resources[id]);
}

template <typename T, typename... Args>
inline std::shared_ptr<T> Resources::Create(Args&&... args)
{
    auto res = std::make_shared<T>(std::forward<Args>(args)...);
    const std::string& path = res->m_path;
    assert(!path.empty());  // Generated resources must have a path set in the constructor
    const auto id = std::hash<std::string>()(path);
    assert(!Find<T>(id));  // Resource with this id already exists (shouldn't happen)

    m_resources[id] = res;
    return std::dynamic_pointer_cast<T>(m_resources[id]);
}

template <typename T>
inline std::shared_ptr<T> Resources::Find(size_t id)
{
    auto it = m_resources.find(id);
    if (it != m_resources.end()) return std::dynamic_pointer_cast<T>(it->second);
    return std::shared_ptr<T>();
}

}  // namespace bee