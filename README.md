# skewTCloudSim
This project is a weather simulation created in C++.  
It has been a project of mine for over a year, and I enjoy working on it.  
A template was used for the rendering at the start, the simulation and the code around it is fully from scratch.  

### Extra Info
How does it work?  
The project is mainly a fluid simulation with extra rules applied to it.  
Rules like when clouds should happen or rain should start to fall.  

The simulation code is done on the GPU, using CUDA (which is hipified to also support AMD GPUs).  


Feel free to reach out for further questions or remarks. 


## Library or Executable
This project is made as library, usable in other projects to create and render clouds.  
Yet, it has many features of its own, which is why it is also possible to use as executable.  
The executable has another simple project compiled with the library which just calls the functions from the library.


## Install

### Cloning

clone the source code into any empty folder.

This can be done simply by right clicking the empty folder and opening a terminal,  
use the following command to clone the source code into the folder:
```
git clone CLONE_URL
```
(Of course replace CLONE_URL with the copied URL)

Now that we have the source code, we can start compiling it into our desired result.

### Compiling

There are multiple compilation routes, they differ based on GPU hardware, build target and configuration.  
To make it easy to get the correct set of commands, the next image shows which path to take based on your desires. 

![alt text](ReadMePics/CMAKEBuildSchemetic.png)

You can compile the library using the CMakelist.txt with presets defined in the CMakePresets.json file.

use the settings from the image to compile the source code.  
For example, for an NVIDIA executable, you use the preset 'NVIDIA-EXE' for both the first and second step, 'build_exe' for the build location, 'Release' as configuration and 'dist_exe' as result location.   
For the executables, you can also get them for debug using the 'NVIDIA-Debug' or 'AMD-Debug' preset together with --config being 'Debug'.


#### **1.**   
Open a terminal at the location of the main CMakelist.txt file at the root.

#### **2.**  
run the **make** command: 
```
cmake --preset NVIDIA-EXE
```
This will build the library into the 'build_exe' folder for visual studio 2022 and cuda version 12.9.

#### **3.**  
Now actually run the **build** using the command:
```
cmake --build build_exe --preset NVIDIA-EXE
```
This command will compile and build the program. 

Note: for NVIDIA, a warning like **warning C4005** about **macro redefinition** may occure quite a lot, this is not harmful for the application. 

#### **4.**   
**Install** the final result giving the lib and exe together with assets:
```
cmake --install build_exe --config Release --prefix dist_exe
```
This will install the build with the Release configuration inside a new folder called 'dist_exe'.



<br>

**<span style="color:yellow">If compiling for AMD fails</span>**, one reason could be that visual studio 2026 (or another version) is building instead of visual studio 2022.  
To (hopefully) fix this, run the bat file located in the same folder in the terminal:
```
build_amd_exe.bat
or
build_amd_lib.bat
```
The bat file searches for visual studio 2022 being installed and executes the vcvars64.bat in the visual studio location.  
This basically loads the visual studio 2022 compiler into this terminal.

This makes sure the preset is run with visual studio 2022.
If it does not work, contact me with the issue, so I may (help) resolve it.

**If using the .bat, you may skip step 3. and go to step 4 istead.**

<br><br>

TODO: outdated?
### Files to manually include

This library makes use of glfw, OpenGL, and ImGui. 
Thus, the following files are expected to be compiled with your own project:  

- [ImGui Github](https://github.com/ocornut/imgui)  
imgui.cpp  
imgui_draw.cpp  
backend/imgui_impl_glfw.cpp  
backend/imgui_impl_opengl3.cpp  
imgui_tables.cpp  
imgui_widgets.cpp  
- [ImGuizmo Github](https://github.com/cedricguillemet/imguizmo)  
src/ImGuizmo.cpp  
- [ImPlot Github](https://github.com/epezent/implot)  
implot.cpp  
implot_items.cpp  
- Glad (download from https://glad.dav1d.de)  
external/glad/src/glad.c  



## Important

**Assets**  
Make sure the 'assets' folder next to the library is also copied over to the working directory.  
This will be done automatically when compiling, but make sure it succeeded.
These assets are used to save ImGui data and has a custom font which is possibly used.  

**Depth Buffer**  
The depth buffer has to be logarithmic (for now) to make sure that the far plane can be extended much further. 
This means that for your possible shaders, you will have to use some different code: [StackOverflow discussion](https://stackoverflow.com/questions/22364930/logarithmic-depth-buffer-opengl) on how to do it.


## Limitations

- As the clouds are post-processing, it is not possible to show the clouds in any reflective objects you draw.

- The shadows from clouds do not take into account blocking of other geometry, making the underside of object also have shadows from clouds. 
