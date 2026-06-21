@echo off
:: Locate Visual Studio installation via vswhere
set "VS_PATH="
for /f "usebackq tokens=*" %%i in (`where vswhere 2^>nul`) do set "VSWHERE=%%i"
if defined VSWHERE (
    for /f "usebackq tokens=*" %%i in (`"%%VSWHERE%" -latest -property installationPath`) do set "VS_PATH=%%i"
)
if not defined VS_PATH (
    :: Fallback: check common VS2022 paths
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional"
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise"
)
if not defined VS_PATH (
    echo Error: Could not find Visual Studio installation. Install VS2022 or place vswhere.exe in PATH.
    exit /b 1
)
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64

:: Ensure PowerShell is in PATH (required by vcpkg applocal step)
where powershell.exe >nul 2>&1 || set "PATH=%PATH%;%SystemRoot%\System32\WindowsPowerShell\v1.0\"

:: Build from the script's directory
cd /d "%~dp0"
if not exist build mkdir build
cd build
cmake -A x64 -DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe ..
cmake --build . --config Release
