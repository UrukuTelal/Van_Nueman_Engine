@echo off
call "C:\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
echo AFTER_VCVARS
where cmake
where cl.exe
cd /d "C:\Projects\Van_Nueman_Engine\build"
cmake .. -DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe
cmake --build . --config Release
echo BUILD_DONE
pause
