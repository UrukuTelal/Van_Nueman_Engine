@echo off
call "C:\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d "C:\Projects\Van_Nueman\Van_Nueman_Engine"
rmdir /s /q build 2>nul
mkdir build
cd build
cmake -G "Ninja" -DCMAKE_MAKE_PROGRAM="C:\Projects\Van_Nueman\Van_Nueman_Engine\ninja.exe" -DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe ..
ninja
