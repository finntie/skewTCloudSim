@echo off
setlocal

rem This bat file locates the visual studio 2022 program and runs the internal bat file to make the current default VS use 2022 instead of possibly something else.

rem Finding the VS 2022 install folder (only version 17)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`^""%VSWHERE%" -version "[17.0,18.0)" -property installationPath^"`) do set "VS22=%%i"

if not defined VS22 (
    echo Visual Studio 2022 was not found. Install it with the C++ workload.
    exit /b 1
)

rem Load the VS 2022 compiler environment into this window
call "%VS22%\VC\Auxiliary\Build\vcvars64.bat"

rem Configure and build using your preset
cmake --preset AMD-EXE || exit /b 1
cmake --build build_AMD_exe --preset AMD-EXE