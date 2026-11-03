@echo off
setlocal
cd /d "%~dp0"

set "LOG=%~dp0install.log"
echo [%date% %time%] install start > "%LOG%"

set "NPPDIR=C:\Program Files\Notepad++"
set "SRC=%~dp0build\NppWordCount.dll"
set "DEST=%NPPDIR%\plugins\NppWordCount"

if not exist "%SRC%" (
    echo ERROR: DLL not found: %SRC% >> "%LOG%"
    exit /b 1
)
if not exist "%NPPDIR%\notepad++.exe" (
    echo ERROR: Notepad++ not found: %NPPDIR% >> "%LOG%"
    exit /b 1
)

if not exist "%DEST%" mkdir "%DEST%" >> "%LOG%" 2>&1

rem 如果 Notepad++ 正在运行，旧 DLL 被占用，先改名再复制；重启 NPP 后生效
if exist "%DEST%\NppWordCount.dll" (
    move /y "%DEST%\NppWordCount.dll" "%DEST%\NppWordCount.dll.old" >> "%LOG%" 2>&1
)
copy /y "%SRC%" "%DEST%\NppWordCount.dll" >> "%LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: copy failed >> "%LOG%"
    exit /b 1
)
del /q "%DEST%\NppWordCount.dll.old" >> "%LOG%" 2>&1

echo OK: %DEST%\NppWordCount.dll >> "%LOG%"
exit /b 0
