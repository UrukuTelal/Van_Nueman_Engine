@echo off
call "C:\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d "C:\Projects\Van_Nueman\Van_Nueman_Engine\build"
C:\Projects\Van_Nueman\Van_Nueman_Engine\ninja.exe %*
