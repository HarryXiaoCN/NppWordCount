@echo off
setlocal
cd /d "%~dp0"

set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo [ERROR] vcvars64.bat not found. Install VS 2022 Build Tools or edit VCVARS in build.bat.
    exit /b 1
)

call "%VCVARS%" >nul
if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC environment.
    exit /b 1
)

if not exist build mkdir build

echo [1/3] Building plugin DLL ...
cl /nologo /utf-8 /std:c++17 /EHsc /O2 /MT /W4 /DUNICODE /D_UNICODE /Fo:build\ /LD src\NppWordCount.cpp src\WordCountCore.cpp /link /OUT:build\NppWordCount.dll /IMPLIB:build\NppWordCount.lib /DLL
if errorlevel 1 goto fail

echo [2/3] Building unit tests ...
cl /nologo /utf-8 /std:c++17 /EHsc /O2 /MT /W4 /Fo:build\ /Fe:build\test_core.exe tests\test_core.cpp src\WordCountCore.cpp
if errorlevel 1 goto fail

echo [3/3] Running unit tests ...
build\test_core.exe
if errorlevel 1 goto fail

echo.
echo Build OK: build\NppWordCount.dll
echo Install: run install.bat as administrator
exit /b 0

:fail
echo.
echo Build FAILED.
exit /b 1
