#include "tools/shader_preprocessor.hpp"

#include <regex>
#include <sstream>

#include "core/engine.hpp"
#include "core/fileio.hpp"
#include "tools/log.hpp"
#include "tools/tools.hpp"

#define ENABLE_PROFILING 0
#define ENABLE_CACHING 1

#if ENABLE_PROFILING
#endif

using namespace bee;
using namespace std;

namespace
{
const regex sIncludeRegex = regex("^[ ]*#[ ]*include[ ]+[\"<](.*)[\">].*");
}  // anonymous namespace

ShaderPreprocessor::ShaderPreprocessor() { _searchPaths.push_back(""); }

string ShaderPreprocessor::Read(bee::FileIO::Directory directory, const string& path)
{
    set<string> includeTree;
    return ParseRecursive(directory, path, "", includeTree);
}

// Based on
// https://www.opengl.org/discussion_boards/showthread.php/169209-include-in-glsl
string ShaderPreprocessor::ParseRecursive(bee::FileIO::Directory directory,
                                          const string& path,
                                          const string& parentPath,
                                          set<string>& includeTree)
{
    string fullPath = parentPath.empty() ? path : parentPath + "/" + path;

    if (includeTree.count(fullPath))
    {
        Log::Error("Circular include found! Path: {}", path.c_str());
        return string();
    }

    includeTree.insert(fullPath);

#if ENABLE_CACHING
    if (StringEndsWith(fullPath, ".glsl"))
    {
        const auto cachedIt = _cachedSources.find(path);
        if (cachedIt != _cachedSources.end())
        {
            return cachedIt->second;
        }
    }
#endif

    // fullPath = Engine.FileIO().GetPath(directory, fullPath);
    string parent = GetParentPath(fullPath);
    string inputString = Engine.FileIO().ReadTextFile(directory, fullPath);
    if (inputString.empty())
    {
        Log::Error("Shader file not found! Path: {}", fullPath.c_str());
        return string();
    }

    stringstream input(move(inputString));
    stringstream output;

    // go through each line and process includes
    string line;
    smatch matches;
    size_t lineNumber = 1;
    string dbg;
    while (getline(input, line))
    {
        dbg = output.str();
        if (regex_search(line, matches, sIncludeRegex))
        {
            auto includeFile = ParseRecursive(directory, matches[1].str(), parent, includeTree);
            output << includeFile;
            dbg = output.str();
            output << "#line " << lineNumber << endl;
            dbg = output.str();
        }
        else if (StringStartsWith(line, "#extension GL_GOOGLE_include_directive : require"))
        {
            output << std::endl;
        }
        else
        {
            if (!line.empty() && line[0] != '\0')  // Don't null terminate
                output << line;
        }
        output << endl;
        lineNumber++;
        dbg = output.str();
    }

#if ENABLE_CACHING
    if (StringEndsWith(fullPath, ".glsl"))
    {
        _cachedSources[path] = output.str();
    }
#endif

    string outputString = output.str();
    return outputString;
}

string ShaderPreprocessor::GetParentPath(const string& path)
{
    // Implementation base on:
    // http://stackoverflow.com/questions/28980386/how-to-get-file-name-from-a-whole-path-by-c

    string parent = "";
    string::size_type found = path.find_last_of("/");

    // if we found one of this symbols
    if (found != string::npos) parent = path.substr(0, found);

    return parent;
}
