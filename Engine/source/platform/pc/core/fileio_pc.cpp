#include "core/fileio.hpp"
#include "tools/log.hpp"

#include <filesystem>
#include <fstream>

using namespace bee;
using namespace std;

FileIO::FileIO()
{
    Paths[Directory::Assets] = "assets/";
    Paths[Directory::SharedAssets] = "assets/";
    Paths[Directory::None] = "";

    // TODO:
    // Paths[Directory::SaveFiles]
}

FileIO::~FileIO() = default;
