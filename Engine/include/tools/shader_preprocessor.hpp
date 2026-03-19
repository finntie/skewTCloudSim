#pragma once

// Portions have been adapted from code written by Paul Houx, Simon Geilfus, Richard Eakin
// Check: https://gist.github.com/richardeakin/f67a696cfd1f4ef3a816

#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/fileio.hpp"

namespace bee
{

class ShaderPreprocessor
{
public:
    ShaderPreprocessor();
    std::string Read(bee::FileIO::Directory directory, const std::string& path);

private:
    std::string ParseRecursive(bee::FileIO::Directory directory,
                               const std::string& path,
                               const std::string& parentPath,
                               std::set<std::string>& includeTree);
    static std::string GetParentPath(const std::string& path);
    std::vector<std::string> _searchPaths;
    std::map<std::string, std::string> _cachedSources;
};

}  // namespace bee
