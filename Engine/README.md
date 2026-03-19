<div align="center">

# 🐝 Breda Educational Engine (BEE)

</div>

BEE is an ongoing project of giving students in year 2 a solid foundation to start their projects with. It also aims to provide scaffolding in a way that projects can from different students can be integrated later on. As such BEE is not a full featured game engine. BEE is only a year old and some aspects are either in flux or buggy (it is however better tested than most student projects).

## 📖 Docs

This documentation goes over the basics of using BEE.

### 🚀 Running BEE
BEE should build cleanly and run  by just using the Visual Studio Solution file. It has support for PC and PS5. While the project started with the aim of having all game-play code in LUA, this is not currently the case or the trajectory. At current the sample games are directly embedded into the engine. You can choose which one to run by passing command line arguments to the executable. These samples should serve as a starting point into exploring how BEE works. Check the `main.cpp` file to see which samples are included with your version of BEE.

### 📁 Folder Structure
BEE aims to demonstrate good usage of folder structure for a small size project. BEE is structured to be used as a library in the future. The root folder should look like this:
- **include** - All public header files (`.hpp` extension). Don not add source files here.
- **source** - All source files (`.cpp` extension or shaders). Do not add headers here (unless they are truly *private*)
- **external** - All external dependencies. Some files here need to be *compiled*.
- **assets** - All game assets and shader libs for the Prospero.
- **properties** - Property sheets that are use to configure the project. Make use of these whenever you can.
- **script** - Python scripts to maintain the project.

The *include* and *source* folders have a similar structure and they map to various features of the engine.
- **core** - Core BEE facilities, like file IO, the ECS and the resource system.
- **ai** - Navigation system.
- **graph** - General graph based data structures and algorithms.
- **math** - Small library of math tools.
- **physics** - A custom 2D physics engine.
- **rendering** - Platform independent parts of the rendering code.
- **platform** - Code specific to each platform.
    - **pc** - PC specific code.
    - **prospero** - Prospero (PS5) specific code
    - **opengl** - OpenGL code. Not strictly a platform, but can run on multiple platforms as needed.
- **tools** - Everything else. Logging, profiling, engine ui, thread pools and such.

All files and folder use `lower_case` naming. Wile the code is `CamelCase`. Please follow BEE's convention where you can.

### ⚙️ Core
The entry point in `Engine`. This object is an instance of the BEE engine. It is an access point to most facilities offered by BEE. It does not include any headers, so you need to include the headers you will be using. It runs the update loop.

### 🧩 ECS
BEE uses an EnTT as an ECS and that dictates the way one interacts with the engine. Transforms, physics simulation, mesh rendering is all done via simple structs that attached to entities. The `ECS` class does give access to the EnTT registry but also provides `CreateEntity` and more importantly `DeleteEntity`. This allows for entities to be tagged for deletion, but be valid for the rest of the update (frame).

Important: Adding a new component to EnTT can *invalidate* all references to components of that type. This can lead to bugs that are very hard to resolve.

### 🫳 Transforms
BEE has a transform component, with support for hierarchies and naming the entities. Make use of it, rather than making your own.

### 🧺 Resources
BEE has a decently documented resource system. It uses shared pointers for reference counting. You need to manually call `CleanUp()`. For more info check the comments in `resources.hpp`. 

### 🕹️ Input
BEE has an input API  that supports controllers, keyboard and mouse. It's well documented with comments.

### 💾 File IO
BEE has a file io API that you should use to access data on disk. Prefer that over accessing files directly. The API is also well documented.

### 📺 Device
You can access the device API via the Engine. This API is different per platform, but it gives you access to the window size on both platforms.

### 🌈 Multi-platform support
BEE is made for PC and PS5 at current. Other platforms could be added in the future. This has impact on the how the code is structured, but it does not apply the same solution to the problem everywhere. Some classes a completely different per platform and only share the header file (for example image.hpp):

```
#pragma once
#if defined(BEE_PLATFORM_PC)
#include "platform/opengl/image_gl.hpp"
#elif defined(BEE_PLATFORM_PROSPERO)
#include "platform/prospero/rendering/image_prospero.hpp"
#endif
```

Other places, like the `DebugRenderer` are using the private implementation pattern. Yet other, like `FileIO` are simply give the definition on some methods  at different `cpp` files. There is not one size fits all for this challenge.

### 📷 Rendering
BEE has a basic PBR renderer. While it lacks advanced features, it does load and render GLTF models. The model loading is a two step process, where the Model is loaded as a resource and instantiated into the scene manually as needed. It can also be instantiated as a child to an existing entity.

### 📜 Logging
BEE has a logging functionality. Prefer that over printf or cout.

### 🔎 Inspector
BEE has a inspector UI that can be exteded to act as an editor
Automatic inspection of complex types is not supported at current.

### 🧶 Serialization
BEE has a serialization API that makes use of the `visit_struct` library for run-time reflection. We have run into some limitations to the system when using it with complex types. If you find a solution to this problem, please let us know.

### 🧑‍⚕️ Static Code Analysis and Formatting
BEE uses clang tools for linting. 

Formatting is done through [ClangFormat](https://clang.llvm.org/docs/ClangFormat.html). A `.clang-format` file is provided in the root of Bee. You can run formatting on all files by running `format.bat` in your command line.

Static Code Analysis is done through [ClangTidy](https://clang.llvm.org/extra/clang-tidy/). A `.clang-tidy` file is provided in the root of Bee, which defines the set of rules we check for. You can run ClangTidy per platform by running `python script\tidy_with_compile_commands.py --platform x64` for PC or with `--platform Prospero`. This script will create folders named `tidy\[Platform]` based on the `--platform` provided as argument. In these new folders you can find all the suggestions from running ClangTidy (which are also printed in the command line). 

By default the checks target `bee.sln`, but you can also target another `.sln` or `vcxproj` file by providing it as argument like so: `python script\tidy_with_compile_commands.py --platform x64 --target bee.vcxproj`

> [!WARNING]  
> This requires the following programs to be installed and added to your `PATH`:
>   + Python 3+ https://www.python.org/downloads/ or https://www.anaconda.com/download/; *anaconda is recommended if you plan on developing with Python
>   + Install llvm clang 18.1.8 https://github.com/llvm/llvm-project/releases/tag/llvmorg-18.1.8, on windows `LLVM-18.1.8-win64.exe`
>   + MSBuild should already be installed via Visual Studio but it has to be added to your PATH, e.g. `C:\Program Files\Microsoft Visual Studio\2022\Community\Msbuild\Current\Bin\`

`ClangTidy` and `ClangFormat` are run as part of the CI on Pull Request.

[🏄‍♀️ GitHub Workflow Guide](docs/github.md)