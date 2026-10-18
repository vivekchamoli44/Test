@echo off
setlocal
echo ========================================================
echo Building AI-Powered Early Health Detection Backend (C++)
echo ========================================================

set "GCC_DIR=C:\Users\VIVEK\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64"
set "GCC_BIN=%GCC_DIR%\bin\g++.exe"
if not exist "%GCC_BIN%" set "GCC_BIN=g++"

set "MYSQL_INC=C:\Program Files\MySQL\MySQL Server 8.0\include"
set "MYSQL_LIB=C:\Program Files\MySQL\MySQL Server 8.0\lib"

if exist "%MYSQL_LIB%\libmysql.dll" (
    copy /y "%MYSQL_LIB%\libmysql.dll" . >nul
)

"%GCC_BIN%" -std=c++17 -O2 -static-libgcc -static-libstdc++ -Iinclude -I"%MYSQL_INC%" -L. -L"%MYSQL_LIB%" src\main.cpp -o server.exe -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic -lmysql -lws2_32 -lmswsock -lwinhttp

if %ERRORLEVEL% equ 0 (
    echo [BUILD SUCCESS] server.exe created successfully with static C++ runtimes!
) else (
    echo [BUILD FAILED] Compilation failed.
)
endlocal
