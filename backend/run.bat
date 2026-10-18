@echo off
setlocal enabledelayedexpansion

set "NEED_BUILD=0"
if not exist server.exe (
    echo server.exe not found! Building first...
    set "NEED_BUILD=1"
) else (
    for /f %%i in ('powershell -NoProfile -Command "try { ((Get-ChildItem src\*.cpp,src\*.h | Measure-Object -Property LastWriteTime -Maximum).Maximum -gt (Get-Item server.exe).LastWriteTime) } catch { $true }"') do set "SRC_NEWER=%%i"
    if /i "!SRC_NEWER!"=="True" (
        echo Source files changed since last build. Rebuilding...
        set "NEED_BUILD=1"
    )
)

if "!NEED_BUILD!"=="1" (
    call .\build.bat
    if errorlevel 1 (
        echo Build failed, aborting.
        exit /b 1
    )
)

echo Starting AI Early Health Detection Center Server...
.\server.exe
endlocal
