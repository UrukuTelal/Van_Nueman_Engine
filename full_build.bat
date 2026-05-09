@echo off
call "C:\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b %errorlevel%

cd /d "C:\Projects\Van_Nueman\Van_Nueman_Engine"

echo ==== Removing old build dir ====
rmdir /s /q build 2>nul

echo ==== mkdir build ====
mkdir build

echo ==== Configuring CMake ====
cd build
cmake -G Ninja -DCMAKE_MAKE_PROGRAM="C:\Projects\Van_Nueman\Van_Nueman_Engine\ninja.exe" ..
if errorlevel 1 exit /b %errorlevel%

echo ==== Building ====
C:\Projects\Van_Nueman\Van_Nueman_Engine\ninja.exe
